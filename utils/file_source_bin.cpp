#include <gst/gst.h>
#include "file_source_bin.hpp"

static void on_pad_added(G_GNUC_UNUSED GstElement *src, GstPad *new_pad, gpointer user_data)
{
    GstElement *h264parser = (GstElement *)user_data;
    GstPad *sink_pad = gst_element_get_static_pad(h264parser, "sink");

    if (gst_pad_is_linked(sink_pad))
    {
        g_object_unref(sink_pad);
        return;
    }

    if (GST_PAD_LINK_FAILED(gst_pad_link(new_pad, sink_pad)))
        g_printerr("Failed to link demuxer and parser.\n");
    else
        g_print("Demuxer and parser linked.\n");

    g_object_unref(sink_pad);
}

void setup_file_src_bin(FileSrcBin *file_src_bin, SrcBin *src_bin, const std::string &location)
{
    // Create elements
    file_src_bin->filesrc = gst_element_factory_make("filesrc", "file-source");
    file_src_bin->demuxer = gst_element_factory_make("qtdemux", "demuxer");
    file_src_bin->h264parse = gst_element_factory_make("h264parse", "h264parse");
    file_src_bin->decoder = gst_element_factory_make("nvv4l2decoder", "nvv4l2-decoder");
    file_src_bin->bin = gst_bin_new("file-source-bin");

    // Set file source location
    g_object_set(G_OBJECT(file_src_bin->filesrc), "location", location.c_str(), NULL);

    // Add elements to the bin
    gst_bin_add_many(GST_BIN(file_src_bin->bin),
                     GST_ELEMENT(file_src_bin->filesrc),
                     GST_ELEMENT(file_src_bin->demuxer),
                     GST_ELEMENT(file_src_bin->h264parse),
                     GST_ELEMENT(file_src_bin->decoder),
                     NULL);

    // Link filesrc to demuxer
    if (!gst_element_link_many(file_src_bin->filesrc, file_src_bin->demuxer, NULL))
    {
        g_printerr("Elements could not be linked. Exiting.\n");
        return;
    }

    // Connect demuxer pad-added signal
    g_signal_connect(file_src_bin->demuxer, "pad-added", G_CALLBACK(on_pad_added), file_src_bin->h264parse);

    // Link h264parse to decoder
    if (!gst_element_link_many(file_src_bin->h264parse, file_src_bin->decoder, NULL))
    {
        g_printerr("Elements could not be linked. Exiting.\n");
        return;
    }

    // Add file source bin to the parent bin
    gst_bin_add(GST_BIN(src_bin->bin), file_src_bin->bin);

    // Create and add ghost pad
    GstPad *srcpad_decoder = gst_element_get_static_pad(file_src_bin->decoder, "src");
    GstPad *srcpad_file_bin = gst_ghost_pad_new("src", srcpad_decoder);
    gst_pad_set_active(srcpad_file_bin, TRUE);

    if (!gst_element_add_pad(file_src_bin->bin, srcpad_file_bin))
    {
        g_printerr("Failed to add ghost pad in file source bin\n");
        return;
    }

    // Link ghost pad to nvstreammux sink pad
    GstPad *sinkpad0 = gst_element_get_request_pad(src_bin->nvstreammux, "sink_0"); // Todo: auto-increment pad names

    if (!sinkpad0)
    {
        g_printerr("Failed to request sink pad from nvstreammux. Exiting.\n");
        return;
    }

    if (gst_pad_link(srcpad_file_bin, sinkpad0) != GST_PAD_LINK_OK)
    {
        g_printerr("Failed to link srcpad_bin to nvstreammux sink pad. Exiting.\n");
        gst_object_unref(sinkpad0);
        return;
    }

    // Clean up references
    gst_object_unref(sinkpad0);
}
