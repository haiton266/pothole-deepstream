#include <gst/gst.h>
#include "file_sink_bin.hpp"

void setup_file_sink_bin(FileSinkBin *file_sink_bin, SinkBin *sink_bin, const std::string &location)
{
    file_sink_bin->bin = gst_bin_new("file-sink-bin");

    file_sink_bin->queue = gst_element_factory_make("queue", "queue");
    file_sink_bin->parser = gst_element_factory_make("h264parse", "h264parse");
    file_sink_bin->qtmux = gst_element_factory_make("qtmux", "qtmux");
    file_sink_bin->filesink = gst_element_factory_make("filesink", "filesink");

    g_object_set(G_OBJECT(file_sink_bin->filesink), "location", location.c_str(), NULL);

    gst_bin_add_many(GST_BIN(file_sink_bin->bin),
                     file_sink_bin->queue,
                     file_sink_bin->parser,
                     file_sink_bin->qtmux,
                     file_sink_bin->filesink,
                     NULL);

    if (!gst_element_link_many(file_sink_bin->queue, file_sink_bin->parser, file_sink_bin->qtmux, file_sink_bin->filesink, NULL))
    {
        g_printerr("Elements could not be linked. Exiting.");
        return;
    }

    gst_bin_add(GST_BIN(sink_bin->bin), file_sink_bin->bin);

    GstPad *sinkpad_queue =
        gst_element_get_static_pad(file_sink_bin->queue, "sink");
    GstPad *sinkpad_file_bin = gst_ghost_pad_new("sink", sinkpad_queue);
    gst_pad_set_active(sinkpad_file_bin, TRUE);
    if (!gst_element_add_pad(file_sink_bin->bin, sinkpad_file_bin))
    {
        g_printerr("Failed to add ghost pad in file sink bin\n");
        return;
    }

    // Request a pad from tee
    GstPad *srcpad0 = gst_element_get_request_pad(sink_bin->tee, "src_0"); // Todo: auto increment

    if (!srcpad0)
    {
        g_printerr("Failed to request src pad from tee. Exiting.\n");
        return;
    }

    if (gst_pad_link(srcpad0, sinkpad_file_bin) != GST_PAD_LINK_OK)
    {
        g_printerr("Failed to link tee src pad to sink pad. Exiting.\n");
        gst_object_unref(srcpad0);
        return;
    }

    // Clean up references
    gst_object_unref(srcpad0);
}