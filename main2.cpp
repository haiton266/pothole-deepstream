#include <gst/gst.h>
#include "main.hpp"
#include <time.h>
#include <yaml-cpp/yaml.h>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <string>
#include <cuda_runtime_api.h>
#include <opencv2/opencv.hpp>

#include "gstnvdsmeta.h"
#include "nvbufsurface.h"
#include "nvds_obj_encode.h"
#include "nvbufsurftransform.h"

#include "common_util.hpp"
#include "file_source_bin.hpp"
#include "file_sink_bin.hpp"
#include "rtsp_sink_bin.hpp"
#include "webcam_source_bin.hpp"
#include "message_broker_bin.hpp"

Pipeline g_pipeline_program;
const size_t MAX_IDS = 100;
const int INTERVAL_SAVE_IMAGE = 5;
int previous_frame = -1 * INTERVAL_SAVE_IMAGE;

gint g_gpu_id = 0;
NvBufSurface *g_inter_buf = nullptr;
cudaStream_t g_cuda_stream;

void handle_sigint(int sig)
{
    if (g_pipeline_program.pipeline)
    {
        g_print("Caught SIGINT, sending EOS...\n");
        gst_element_send_event(g_pipeline_program.pipeline, gst_event_new_eos());
    }
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

    frame_num++;       // Increment once per batch
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

cv::Mat *get_opencv_mat(NvBufSurface &ip_surf, gint idx,
                        gdouble &ratio, gint processing_width,
                        gint processing_height)
{

    NvOSD_RectParams rect_params;

    // Scale the entire frame to processing resolution
    rect_params.left = 0;
    rect_params.top = 0;
    rect_params.width = processing_width;
    rect_params.height = processing_height;

    gint src_left = GST_ROUND_UP_2((int)rect_params.left);
    gint src_top = GST_ROUND_UP_2((int)rect_params.top);
    gint src_width = GST_ROUND_DOWN_2((int)rect_params.width);
    gint src_height = GST_ROUND_DOWN_2((int)rect_params.height);

    // Maintain aspect ratio
    double hdest = processing_width * src_height / (double)src_width;
    double wdest = processing_height * src_width / (double)src_height;
    guint dest_width, dest_height;

    if (hdest <= processing_height)
    {
        dest_width = processing_width;
        dest_height = hdest;
    }
    else
    {
        dest_width = wdest;
        dest_height = processing_height;
    }

    // Configure transform session parameters for the transformation
    NvBufSurfTransformConfigParams transform_config_params;
    transform_config_params.compute_mode = NvBufSurfTransformCompute_Default;
    transform_config_params.gpu_id = g_gpu_id;
    transform_config_params.cuda_stream = g_cuda_stream;

    // Set the transform session parameters for the conversions executed in this thread.
    if (auto err = NvBufSurfTransformSetSessionParams(&transform_config_params);
        err != NvBufSurfTransformError_Success)
    {
        g_printerr("NvBufSurfTransformSetSessionParams failed with error {}\n", err);
        return nullptr;
    }

    // Calculate scaling ratio while maintaining aspect ratio
    ratio = MIN(1.0 * dest_width / src_width, 1.0 * dest_height / src_height);

    if ((rect_params.width == 0) || (rect_params.height == 0))
    {
        g_printerr("[get_opencv_mat]:crop_rect_params dimensions are zero");
        return nullptr;
    }

#ifdef __aarch64__
    if (ratio <= 1.0 / 16 || ratio >= 16.0)
    {
        // Currently cannot scale by ratio > 16 or < 1/16 for Jetson
        g_printerr("[get_opencv_mat] ratio {} not in range of [.0625 : 16]", ratio);
        return nullptr;
    }
#endif
    // Set the transform ROIs for source and destination
    NvBufSurfTransformRect src_rect = {(guint)src_top, (guint)src_left, (guint)src_width,
                                       (guint)src_height};
    NvBufSurfTransformRect dst_rect = {0, 0, (guint)dest_width, (guint)dest_height};

    // Set the transform parameters
    NvBufSurfTransformParams transform_params;
    transform_params.src_rect = &src_rect;
    transform_params.dst_rect = &dst_rect;
    transform_params.transform_flag =
        NVBUFSURF_TRANSFORM_FILTER | NVBUFSURF_TRANSFORM_CROP_SRC | NVBUFSURF_TRANSFORM_CROP_DST;
    transform_params.transform_filter = NvBufSurfTransformInter_Default;

    // Memset the memory
    NvBufSurfaceMemSet(g_inter_buf, idx, 0, 0);

    if (auto err = NvBufSurfTransform(&ip_surf, g_inter_buf, &transform_params);
        err != NvBufSurfTransformError_Success)
    {
        g_printerr("NvBufSurfTransform failed with error while converting buffer");
        return nullptr;
    }
    // Map the buffer so that it can be accessed by CPU
    if (NvBufSurfaceMap(g_inter_buf, idx, 0, NVBUF_MAP_READ) != 0)
    {
        return nullptr;
    }

    const auto in_mat =
        cv::Mat(processing_height, processing_width, CV_8UC4,
                g_inter_buf->surfaceList[0].mappedAddr.addr[0], g_inter_buf->surfaceList[0].pitch);

    cv::Mat *image_bgr = new cv::Mat;

    cv::cvtColor(in_mat, *image_bgr, cv::COLOR_RGBA2BGR);

    if (NvBufSurfaceUnMap(g_inter_buf, idx, 0))
    {
        return nullptr;
    }

    return image_bgr;
}

static GstPadProbeReturn
nvdsosd_src_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer ctx)
{
    static std::unordered_map<int, bool> recent_ids;
    static std::queue<int> id_queue;

    GstBuffer *buf = (GstBuffer *)info->data;
    GstMapInfo inmap = GST_MAP_INFO_INIT;
    if (!gst_buffer_map(buf, &inmap, GST_MAP_READ))
    {
        GST_ERROR("input buffer mapinfo failed");
        return GST_PAD_PROBE_OK;
    }
    NvBufSurface *ip_surf = (NvBufSurface *)inmap.data;
    gst_buffer_unmap(buf, &inmap);

    NvDsMetaList *l_frame = NULL;
    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buf);

    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
         l_frame = l_frame->next)
    {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(l_frame->data);
        std::string image_path = "./images_saved/img" + std::to_string(frame_meta->frame_num) + ".jpg";

        if (!frame_meta || !frame_meta->obj_meta_list)
            continue;

        bool is_save_and_pub = false;
        for (NvDsMetaList *obj_list = frame_meta->obj_meta_list;
             obj_list != nullptr;
             obj_list = obj_list->next)
        {
            NvDsObjectMeta *obj_meta = (NvDsObjectMeta *)(obj_list->data);
            if (!obj_meta)
                continue;

            int object_id = obj_meta->object_id;

            // Nếu object_id đã tồn tại, bỏ qua
            if (recent_ids.find(object_id) != recent_ids.end())
                continue;

            // Thêm ID mới vào map và queue
            recent_ids[object_id] = true;
            id_queue.push(object_id);

            // Nếu số lượng ID vượt quá MAX_IDS, xóa ID lâu nhất
            if (id_queue.size() > MAX_IDS)
            {
                int old_id = id_queue.front();
                id_queue.pop();
                recent_ids.erase(old_id);
            }

            is_save_and_pub = true;
            if (is_save_and_pub and frame_meta->frame_num - previous_frame >= INTERVAL_SAVE_IMAGE)
            {
                // Setting publish to Kafka broker
                strncpy(obj_meta->obj_label, image_path.c_str(), sizeof(obj_meta->obj_label) - 1);
                obj_meta->obj_label[sizeof(obj_meta->obj_label) - 1] = '\0';
            }
        }

        if (is_save_and_pub and frame_meta->frame_num - previous_frame >= INTERVAL_SAVE_IMAGE)
        {
            previous_frame = frame_meta->frame_num;

            /*Main Function Call */
            g_print("Save image: %s\n", image_path.c_str());

            GstVideoInfo video_info = {};

            GstCaps *caps = gst_pad_get_current_caps(pad);
            if (!gst_video_info_from_caps(&video_info, caps))
            {
                g_printerr("[ProcessFrame] failed to get video_info \n");
                return (GstPadProbeReturn)GST_FLOW_ERROR;
            }
            gst_caps_unref(caps);

            if (g_inter_buf == nullptr)
            {
                g_printerr("[ProcessFrame] initializing Frame buffer \n");
                NvBufSurfaceCreateParams create_params;
                create_params.gpuId = g_gpu_id;
                create_params.width = video_info.width;
                create_params.height = video_info.height;
                create_params.size = 0;
                create_params.colorFormat = NVBUF_COLOR_FORMAT_RGBA;
                create_params.layout = NVBUF_LAYOUT_PITCH;
#ifdef __aarch64__
                create_params.memType = NVBUF_MEM_SURFACE_ARRAY;
#else
                create_params.memType = NVBUF_MEM_CUDA_UNIFIED;
#endif
                if (NvBufSurfaceCreate(&g_inter_buf, 1, &create_params) != 0)
                {
                    g_printerr("Error: Could not allocate internal buffer for dsexample\n");
                    return (GstPadProbeReturn)GST_FLOW_ERROR;
                }
            }

            cv::Mat frame;
            const int index = 0;
            double scale_ratio = 1.0;
            const auto frame_width = ip_surf->surfaceList[0].width;
            const auto frame_height = ip_surf->surfaceList[0].height;

            if (const auto *ret = get_opencv_mat(*ip_surf, index, scale_ratio, frame_width, frame_height); ret == nullptr)
            {
                return (GstPadProbeReturn)GST_FLOW_ERROR;
            }
            else
            {
                frame = *ret;
                delete ret;
            }
            bool success = cv::imwrite(image_path, frame);

            if (success)
            {
                g_print("Image saved successfully to %s\n", image_path.c_str());
            }
            else
            {
                g_print("Error saving image!\n");
            }
        }
    }
    return GST_PAD_PROBE_OK;
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
                 1280,
                 "height",
                 720,
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
    std::string type;
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
            type = "webcam";
            WebcamSrcBin *webcam_src_bin = new WebcamSrcBin();
            setup_webcam_src_bin(webcam_src_bin, src_bin, it->second["device"].as<string>());
        }
    }
    gst_bin_add(GST_BIN(g_pipeline_program->pipeline), src_bin->bin);
    g_pipeline_program->src_bin = src_bin;

    if (type == "webcam")
        g_object_set(G_OBJECT(src_bin->nvstreammux), "live-source", 1, NULL);

    g_print("Source node initialized\n");
    // End of source node ============================================================

    // This is for process node ======================================================
    ProcessBin *process_bin = new ProcessBin();
    process_bin->bin = gst_bin_new("process-bin");

    // This for processing ROI
    YAML::Node nvdspreprocess = config["nvdspreprocess"];
    process_bin->nvdspreprocess = gst_element_factory_make("nvdspreprocess", "nvdspreprocess");
    string pre_config_file_path = nvdspreprocess["config-file-path"].as<string>();

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

    // Set properties
    g_object_set(G_OBJECT(process_bin->nvdspreprocess), "config-file", pre_config_file_path.c_str(), NULL);
    g_object_set(G_OBJECT(process_bin->nvinfer),
                 "config-file-path", config_file_path.c_str(),
                 "input-tensor-meta", 1,
                 NULL);
    if (!set_tracker_properties(process_bin->nvtracker, (gchar *)tracker_config_file_path.c_str()))
    {
        g_printerr("Failed to set tracker properties. Exiting.\n");
        return;
    }

    gst_bin_add_many(GST_BIN(process_bin->bin),
                     process_bin->nvdspreprocess,
                     process_bin->nvinfer,
                     process_bin->nvtracker,
                     process_bin->nvvidconv,
                     process_bin->nvdsosd,
                     NULL);
    gst_element_link_many(process_bin->nvdspreprocess,
                          process_bin->nvinfer,
                          process_bin->nvtracker,
                          process_bin->nvvidconv,
                          process_bin->nvdsosd,
                          NULL);

    // Attach pad probe to the sink pad of nvdsosd
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

    // Attach pad probe to the src pad of nvtracker
    GstPad *osd_src_pad =
        gst_element_get_static_pad(process_bin->nvdsosd, "src");
    if (!osd_src_pad)
    {
        g_printerr("Unable to get nvtracker src pad\n");
    }
    else
    {
        gst_pad_add_probe(osd_src_pad,
                          GST_PAD_PROBE_TYPE_BUFFER,
                          nvdsosd_src_pad_buffer_probe,
                          //   (gpointer)obj_ctx_handle,
                          NULL,
                          NULL);
    }
    gst_object_unref(osd_src_pad);

    // Add src ghost pad to process bin
    GstPad *srcpad_nvdsosd = gst_element_get_static_pad(process_bin->nvdsosd, "src");
    process_bin->src_ghost_pad = gst_ghost_pad_new("src", srcpad_nvdsosd);
    gst_pad_set_active(process_bin->src_ghost_pad, TRUE);
    if (!gst_element_add_pad(process_bin->bin, process_bin->src_ghost_pad))
    {
        g_printerr("Failed to add src_ghost_pad to process_bin \n");
    }

    // Add sink ghost pad to process bin
    GstPad *sinkpad_nvdspreprocess = gst_element_get_static_pad(process_bin->nvdspreprocess, "sink");
    process_bin->sink_ghost_pad = gst_ghost_pad_new("sink", sinkpad_nvdspreprocess);
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
    MessageBroker *msg_broker_bin = new MessageBroker();
    setup_message_broker(msg_broker_bin, sink_bin);

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
