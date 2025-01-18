#include <gst/gst.h>
#include "main.hpp"
#include <time.h>
#include <yaml-cpp/yaml.h>
#include <chrono>
#include "gstnvdsmeta.h"
#include "file_source_bin.hpp"
#include "file_sink_bin.hpp"
#include "rtsp_sink_bin.hpp"
#include "webcam_source_bin.hpp"

Pipeline g_pipeline_program;


void handle_sigint(int sig) {
    if (g_pipeline_program.pipeline) {
        g_print("Caught SIGINT, sending EOS...\n");
        gst_element_send_event(g_pipeline_program.pipeline, gst_event_new_eos());
    }
}

static gchar *
get_absolute_file_path(gchar *cfg_file_path, gchar *file_path)
{
    gchar abs_cfg_path[PATH_MAX + 1];
    gchar *abs_file_path;
    gchar *delim;

    if (file_path && file_path[0] == '/')
    {
        return file_path;
    }

    if (!realpath(cfg_file_path, abs_cfg_path))
    {
        g_free(file_path);
        return NULL;
    }

    // Return absolute path of config file if file_path is NULL.
    if (!file_path)
    {
        abs_file_path = g_strdup(abs_cfg_path);
        return abs_file_path;
    }

    delim = g_strrstr(abs_cfg_path, "/");
    *(delim + 1) = '\0';

    abs_file_path = g_strconcat(abs_cfg_path, file_path, NULL);
    g_free(file_path);

    return abs_file_path;
}

static gboolean
bus_call(G_GNUC_UNUSED GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop *)data;
    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_EOS:
        g_print("End of stream\n");
        g_main_loop_quit(loop);
        break;
    case GST_MESSAGE_ERROR:
    {
        gchar *debug;
        GError *error;
        gst_message_parse_error(msg, &error, &debug);
        g_printerr("ERROR from element %s: %s\n",
                   GST_OBJECT_NAME(msg->src), error->message);
        if (debug)
            g_printerr("Error details: %s\n", debug);
        g_free(debug);
        g_error_free(error);
        g_main_loop_quit(loop);
        break;
    }
    default:
        break;
    }
    return TRUE;
}

// Pad probe function
static GstPadProbeReturn
nvdsosd_sink_pad_buffer_probe(G_GNUC_UNUSED GstPad *pad,
                              GstPadProbeInfo *info,
                              G_GNUC_UNUSED gpointer u_data)
{
    static int frame_num = 0;
    static time_t start_time = 0;
    static int fps_frame_count = 0;

    if (start_time == 0)
    {
        start_time = time(NULL);
    }

    frame_num++; // Increment once per batch
    fps_frame_count++; // Increment FPS frame count

    time_t current_time = time(NULL);
    double elapsed_time = difftime(current_time, start_time);

    if (elapsed_time >= 1.0)
    {
        double fps = fps_frame_count / elapsed_time;
        g_print("FPS: %.2f\n", fps);
        fps_frame_count = 0;
        start_time = current_time;
    }

    if (frame_num % 1 == 0)
        g_print("Frame number at sink pad of nvdsosd: %d\n", frame_num);

    GstBuffer *buf = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buf)
        return GST_PAD_PROBE_OK;

    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    if (!batch_meta)
        return GST_PAD_PROBE_OK;

    // Map to store colors for each object_id and maintain a list for recent IDs
    static GHashTable *color_map = NULL;
    static GQueue *recent_ids = NULL;
    if (!color_map)
    {
        color_map = g_hash_table_new(g_int_hash, g_int_equal);
        recent_ids = g_queue_new();
    }

    for (NvDsMetaList *frame_list = batch_meta->frame_meta_list;
         frame_list != NULL;
         frame_list = frame_list->next)
    {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(frame_list->data);
        if (!frame_meta || !frame_meta->obj_meta_list)
            continue;

        NvDsDisplayMeta *display_meta = nvds_acquire_display_meta_from_pool(batch_meta);
        if (!display_meta)
            continue;

        for (NvDsMetaList *obj_list = frame_meta->obj_meta_list;
             obj_list != NULL;
             obj_list = obj_list->next)
        {
            NvDsObjectMeta *obj_meta = (NvDsObjectMeta *)(obj_list->data);
            if (!obj_meta)
                continue;

            gpointer color_ptr = g_hash_table_lookup(color_map, &(obj_meta->object_id));
            gfloat *color = (gfloat *)color_ptr;

            if (!color)
            {
                if (g_queue_get_length(recent_ids) >= 100)
                {
                    gpointer old_id = g_queue_pop_head(recent_ids);
                    g_hash_table_remove(color_map, old_id);
                    g_free(old_id);
                }

                gfloat *new_color = g_new(gfloat, 4);
                new_color[0] = g_random_double(); // Red
                new_color[1] = g_random_double(); // Green
                new_color[2] = g_random_double(); // Blue
                new_color[3] = 1.0;               // Alpha

                gint *id_copy = g_new(gint, 1);
                *id_copy = obj_meta->object_id;
                g_hash_table_insert(color_map, id_copy, new_color);
                g_queue_push_tail(recent_ids, id_copy);
                color = new_color;
            }

            obj_meta->rect_params.border_color.red = color[0];
            obj_meta->rect_params.border_color.green = color[1];
            obj_meta->rect_params.border_color.blue = color[2];
            obj_meta->rect_params.border_color.alpha = color[3];
        }

        nvds_add_display_meta_to_frame(frame_meta, display_meta);
    }
    return GST_PAD_PROBE_OK;
}

static gboolean
set_tracker_properties(GstElement *nvtracker, gchar *tracker_config_file)
{
    gboolean ret = FALSE;
    GError *error = NULL;
    gchar **keys = NULL;
    gchar **key = NULL;
    GKeyFile *key_file = g_key_file_new();

    if (!g_key_file_load_from_file(key_file, tracker_config_file, G_KEY_FILE_NONE,
                                   &error))
    {
        g_printerr("Failed to load config file: %s\n", error->message);
        return FALSE;
    }

    keys = g_key_file_get_keys(key_file, CONFIG_GROUP_TRACKER, NULL, &error);
    CHECK_ERROR(error);

    for (key = keys; *key; key++)
    {
        if (!g_strcmp0(*key, CONFIG_GROUP_TRACKER_WIDTH))
        {
            gint width =
                g_key_file_get_integer(key_file, CONFIG_GROUP_TRACKER,
                                       CONFIG_GROUP_TRACKER_WIDTH, &error);
            CHECK_ERROR(error);
            g_object_set(G_OBJECT(nvtracker), "tracker-width", width, NULL);
        }
        else if (!g_strcmp0(*key, CONFIG_GROUP_TRACKER_HEIGHT))
        {
            gint height =
                g_key_file_get_integer(key_file, CONFIG_GROUP_TRACKER,
                                       CONFIG_GROUP_TRACKER_HEIGHT, &error);
            CHECK_ERROR(error);
            g_object_set(G_OBJECT(nvtracker), "tracker-height", height, NULL);
        }
        else if (!g_strcmp0(*key, CONFIG_GPU_ID))
        {
            guint gpu_id =
                g_key_file_get_integer(key_file, CONFIG_GROUP_TRACKER,
                                       CONFIG_GPU_ID, &error);
            CHECK_ERROR(error);
            g_object_set(G_OBJECT(nvtracker), "gpu_id", gpu_id, NULL);
        }
        else if (!g_strcmp0(*key, CONFIG_GROUP_TRACKER_LL_CONFIG_FILE))
        {
            char *ll_config_file = get_absolute_file_path(tracker_config_file,
                                                          g_key_file_get_string(key_file,
                                                                                CONFIG_GROUP_TRACKER,
                                                                                CONFIG_GROUP_TRACKER_LL_CONFIG_FILE, &error));
            CHECK_ERROR(error);
            g_object_set(G_OBJECT(nvtracker), "ll-config-file", ll_config_file, NULL);
        }
        else if (!g_strcmp0(*key, CONFIG_GROUP_TRACKER_LL_LIB_FILE))
        {
            char *ll_lib_file = get_absolute_file_path(tracker_config_file,
                                                       g_key_file_get_string(key_file,
                                                                             CONFIG_GROUP_TRACKER,
                                                                             CONFIG_GROUP_TRACKER_LL_LIB_FILE, &error));
            CHECK_ERROR(error);
            g_object_set(G_OBJECT(nvtracker), "ll-lib-file", ll_lib_file, NULL);
        }
        else
        {
            g_printerr("Unknown key '%s' for group [%s]", *key,
                       CONFIG_GROUP_TRACKER);
        }
    }

    ret = TRUE;
done:
    if (error)
    {
        g_error_free(error);
    }
    if (keys)
    {
        g_strfreev(keys);
    }
    if (!ret)
    {
        g_printerr("%s failed", __func__);
    }
    return ret;
}

void init_config(Pipeline *g_pipeline_program, string deepstream_config_file_path)
{
    YAML::Node config = YAML::LoadFile(deepstream_config_file_path);

    // This is for source node =================================================
    YAML::Node source = config["source"];
    SrcBin *src_bin = new SrcBin();
    src_bin->bin = gst_bin_new("source-bin");
    src_bin->nvstreammux = gst_element_factory_make("nvstreammux", "nvstream-mux");
    g_object_set(G_OBJECT(src_bin->nvstreammux),
                 "batch-size",
                 1,
                 "width",
                 1920,
                 "height",
                 1080,
                 "batched-push-timeout",
                 40000,
                 NULL);
    gst_bin_add(GST_BIN(src_bin->bin), src_bin->nvstreammux);

    GstPad *srcpad_nvstreamux =
        gst_element_get_static_pad(src_bin->nvstreammux, "src");
    src_bin->ghost_pad = gst_ghost_pad_new("src", srcpad_nvstreamux);
    gst_pad_set_active(src_bin->ghost_pad, TRUE);

    if (!gst_element_add_pad(src_bin->bin, src_bin->ghost_pad))
    {
        g_printerr("Failed to add ghost pad in file source bin\n");
    }

    for (YAML::const_iterator it = source.begin(); it != source.end(); ++it)
    {
        string key = it->first.as<string>();
        if (key == "filesrc")
        {
            FileSrcBin *file_src_bin = new FileSrcBin();
            setup_file_src_bin(file_src_bin, src_bin, it->second["location"].as<string>());
        }
        else if (key == "webcam")
        {
            WebcamSrcBin *webcam_src_bin = new WebcamSrcBin();
            setup_webcam_src_bin(webcam_src_bin, src_bin, it->second["device"].as<string>());
        }
    }
    gst_bin_add(GST_BIN(g_pipeline_program->pipeline), src_bin->bin);
    g_pipeline_program->src_bin = src_bin;
    g_print("Source node initialized\n");
    // End of source node ============================================================

    // This is for process node ======================================================
    ProcessBin *process_bin = new ProcessBin();
    process_bin->bin = gst_bin_new("process-bin");

    // This is for inference
    YAML::Node inference = config["nvinfer"];
    string config_file_path = inference["config-file-path"].as<string>();
    process_bin->nvinfer = gst_element_factory_make("nvinfer", "nv-infer");

    // This is for tracker
    YAML::Node tracker = config["nvtracker"];
    string tracker_config_file_path = tracker["config-file-path"].as<string>();
    process_bin->nvtracker = gst_element_factory_make("nvtracker", "nvtracker");

    // Define the rest of the elements
    process_bin->nvvidconv = gst_element_factory_make("nvvideoconvert", "nvvideo-convert");
    process_bin->nvdsosd = gst_element_factory_make("nvdsosd", "nvdsosd");
    process_bin->nvvidconv2 = gst_element_factory_make("nvvideoconvert", "nvvideo-convert-2");
    process_bin->encoder = gst_element_factory_make("nvv4l2h264enc", "nvv4l2h264enc");

    // Set properties
    g_object_set(G_OBJECT(process_bin->nvinfer), "config-file-path", config_file_path.c_str(), NULL);
    if (!set_tracker_properties(process_bin->nvtracker, (gchar *)tracker_config_file_path.c_str()))
    {
         g_printerr("Failed to set tracker properties. Exiting.\n");
         return;
    }

    gst_bin_add_many(GST_BIN(process_bin->bin),
                     process_bin->nvinfer,
                     process_bin->nvtracker,
                     process_bin->nvvidconv,
                     process_bin->nvdsosd,
                     process_bin->nvvidconv2,
                     process_bin->encoder,
                     NULL);
    gst_element_link_many(process_bin->nvinfer,
                          process_bin->nvtracker,
                          process_bin->nvvidconv,
                          process_bin->nvdsosd,
                          process_bin->nvvidconv2,
                          NULL);
    if (!gst_element_link(process_bin->nvvidconv2, process_bin->encoder))
    {
        g_printerr("Failed to link. Exiting.\n");
        return;
    }

    // Attach pad probe to the src pad of nvdsosd
    GstPad *nvdsosd_sink_pad =
        gst_element_get_static_pad(process_bin->nvdsosd, "sink");
    if (!nvdsosd_sink_pad)
    {
        g_printerr("Unable to get nvdsosd sink pad\n");
    }
    else
    {
        gst_pad_add_probe(nvdsosd_sink_pad,
                          GST_PAD_PROBE_TYPE_BUFFER,
                          nvdsosd_sink_pad_buffer_probe,
                          NULL,
                          NULL);
    }
    gst_object_unref(nvdsosd_sink_pad);

    // Add src ghost pad to process bin
    GstPad *srcpad_encoder = gst_element_get_static_pad(process_bin->encoder, "src");
    process_bin->src_ghost_pad = gst_ghost_pad_new("src", srcpad_encoder);
    gst_pad_set_active(process_bin->src_ghost_pad, TRUE);
    if (!gst_element_add_pad(process_bin->bin, process_bin->src_ghost_pad))
    {
        g_printerr("Failed to add src_ghost_pad to process_bin \n");
    }

    // Add sink ghost pad to process bin
    GstPad *sinkpad_nvinfer = gst_element_get_static_pad(process_bin->nvinfer, "sink");
    process_bin->sink_ghost_pad = gst_ghost_pad_new("sink", sinkpad_nvinfer);
    gst_pad_set_active(process_bin->sink_ghost_pad, TRUE);
    if (!gst_element_add_pad(process_bin->bin, process_bin->sink_ghost_pad))
    {
        g_printerr("Failed to add sink_ghost_pad to process_bin \n");
    }

    gst_bin_add(GST_BIN(g_pipeline_program->pipeline), process_bin->bin);
    g_pipeline_program->process_bin = process_bin;
    g_print("Process node initialized\n");
    // End of process node ==============================================================

    // This is for sink node ============================================================
    SinkBin *sink_bin = new SinkBin();
    sink_bin->bin = gst_bin_new("sink-bin");
    sink_bin->tee = gst_element_factory_make("tee", "tee");

    gst_bin_add(GST_BIN(sink_bin->bin), sink_bin->tee);

    GstPad *sinkpad_tee =
        gst_element_get_static_pad(sink_bin->tee, "sink");
    sink_bin->sink_ghost_pad = gst_ghost_pad_new("sink", sinkpad_tee);
    gst_pad_set_active(sink_bin->sink_ghost_pad, TRUE);

    if (!gst_element_add_pad(sink_bin->bin, sink_bin->sink_ghost_pad))
    {
        g_printerr("Failed to add sink_ghost_pad in sink bin\n");
    }

    YAML::Node sink = config["sink"];
    for (YAML::const_iterator it = sink.begin(); it != sink.end(); ++it)
    {
        string key = it->first.as<string>();
        if (key == "filesink")
        {
            FileSinkBin *file_sink_bin = new FileSinkBin();
            setup_file_sink_bin(file_sink_bin, sink_bin, it->second["location"].as<string>());
        }
        else if (key == "rtsp")
        {
            RTSPSinkBin *rtsp_sink_bin = new RTSPSinkBin();
            setup_rtsp_sink_bin(rtsp_sink_bin, sink_bin, it->second["ip"].as<string>(), it->second["port"].as<int>());
        }
    }
    gst_bin_add(GST_BIN(g_pipeline_program->pipeline), sink_bin->bin);
    g_pipeline_program->sink_bin = sink_bin;
    g_print("Sink node initialized\n");
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        g_printerr("Need syntax: %s <config_pipeline.yml>\n", argv[0]);
        return -1;
    }
    else
    {
        g_print("Using config file: %s\n", argv[1]);
    }

    gst_init(&argc, &argv);


    g_pipeline_program.pipeline = gst_pipeline_new("pipeline-0");


    // Set up the SIGINT handler
    signal(SIGINT, handle_sigint);

    init_config(&g_pipeline_program, argv[1]);

    // Link ghost_pad of source bin vs sink_ghost_pad of process bin
    if (gst_pad_link(g_pipeline_program.src_bin->ghost_pad, g_pipeline_program.process_bin->sink_ghost_pad) != 0)
    {
        g_printerr("Failed to link src_ghost_pad to sink_ghost_pad. Exiting.\n");
    }

    // Link src_ghost_pad of process bin vs sink_ghost_pad of sink bin
    if (gst_pad_link(g_pipeline_program.process_bin->src_ghost_pad, g_pipeline_program.sink_bin->sink_ghost_pad) != 0)
    {
        g_printerr("Failed to link src_ghost_pad to sink_ghost_pad. Exiting.\n");
    }

    GMainLoop *main_loop;
    main_loop = g_main_loop_new(NULL, FALSE);
    GstBus *bus;
    bus = gst_element_get_bus(g_pipeline_program.pipeline);
    G_GNUC_UNUSED guint bus_watch_id = gst_bus_add_watch(bus, bus_call, main_loop);
    gst_object_unref(bus);

    GST_DEBUG_BIN_TO_DOT_FILE(
        GST_BIN(g_pipeline_program.pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "pipeline");

    // Calculate time (seconds) to run the pipeline
    auto start = std::chrono::high_resolution_clock::now();

    // Start playing
    gst_element_set_state(g_pipeline_program.pipeline, GST_STATE_PLAYING);
    g_main_loop_run(main_loop);

    // Free resources
    g_main_loop_unref(main_loop);
    gst_object_unref(bus);
    gst_element_set_state(g_pipeline_program.pipeline, GST_STATE_NULL);
    gst_object_unref(g_pipeline_program.pipeline);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    g_print("Time taken to run the pipeline: %f seconds\n", elapsed.count());

    return 0;
}
