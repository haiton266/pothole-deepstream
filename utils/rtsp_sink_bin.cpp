#include "rtsp_sink_bin.hpp"
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/gst.h>

void setup_rtsp_sink_bin(RTSPSinkBin *rtsp_sink_bin, SinkBin *sink_bin, std::string ip, int port)
{
    int temp_port = 8079;
    rtsp_sink_bin->bin = gst_bin_new("rtsp-sink-bin");

    rtsp_sink_bin->queue = gst_element_factory_make("queue", "queue");
    rtsp_sink_bin->rtph264pay = gst_element_factory_make("rtph264pay", "rtph264pay");
    rtsp_sink_bin->udpsink = gst_element_factory_make("udpsink", "udpsink");

    g_object_set(G_OBJECT(rtsp_sink_bin->udpsink),
                 "host", ip.c_str(),
                 "port", temp_port,
                 NULL);

    gst_bin_add_many(GST_BIN(rtsp_sink_bin->bin),
                     rtsp_sink_bin->queue,
                     rtsp_sink_bin->rtph264pay,
                     rtsp_sink_bin->udpsink,
                     NULL);

    if (!gst_element_link_many(rtsp_sink_bin->queue, rtsp_sink_bin->rtph264pay, rtsp_sink_bin->udpsink, NULL))
    {
        g_printerr("Elements could not be linked. Exiting.");
        return;
    }

    gst_bin_add(GST_BIN(sink_bin->bin), rtsp_sink_bin->bin);

    GstPad *sinkpad_queue =
        gst_element_get_static_pad(rtsp_sink_bin->queue, "sink");

    GstPad *sinkpad_rtsp_bin = gst_ghost_pad_new("sink", sinkpad_queue);
    gst_pad_set_active(sinkpad_rtsp_bin, TRUE);
    if (!gst_element_add_pad(rtsp_sink_bin->bin, sinkpad_rtsp_bin))
    {
        g_printerr("Failed to add ghost pad in rtsp sink bin\n");
        return;
    }

    // Request a pad from tee
    GstPad *srcpad0 = gst_element_get_request_pad(sink_bin->tee, "src_0"); // Todo: auto increment
    if (!srcpad0)
    {
        g_printerr("Failed to request src pad from tee. Exiting.\n");
        return;
    }

    if (gst_pad_link(srcpad0, sinkpad_rtsp_bin) != GST_PAD_LINK_OK)
    {
        g_printerr("Failed to link tee src pad to sink pad. Exiting.\n");
        gst_object_unref(srcpad0);
        return;
    }

    // Clean up references
    gst_object_unref(srcpad0);

    // Create RTSP server instance
    GstRTSPServer *server = gst_rtsp_server_new();
    gst_rtsp_server_set_address(server, ip.c_str());
    gst_rtsp_server_set_service(server, std::to_string(port).c_str());

    // Get mount points for server
    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);

    gchar *str = g_strdup_printf("( "
                                 "udpsrc port=%d ! application/x-rtp,encoding-name=H264"
                                 "! rtph264depay ! rtph264pay name=pay0 "
                                 ")",
                                 temp_port);

    // Create media factory and set launch string
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(factory, str);

    // Free the launch string after use
    g_free(str);

    // Attach the media factory to the /stream URL
    gst_rtsp_mount_points_add_factory(mounts, "/stream", factory);

    // Clean up mount points
    g_object_unref(mounts);

    // Attach the server to the default main context and start serving
    gst_rtsp_server_attach(server, NULL);
    g_print("Stream ready at rtsp://0.0.0.0:%d/stream\n", port);
}