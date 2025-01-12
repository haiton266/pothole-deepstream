#include <gst/gst.h>
#include "webcam_source_bin.hpp"

void setup_webcam_src_bin(WebcamSrcBin *webcam_src_bin, SrcBin *src_bin, const std::string &device)
{
    // Create elements
    webcam_src_bin->source = gst_element_factory_make("v4l2src", "v4l2src");
    webcam_src_bin->capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    webcam_src_bin->decoder = gst_element_factory_make("jpegdec", "jpegdec");
    webcam_src_bin->nvvideoconvert = gst_element_factory_make("nvvideoconvert", "nvvideoconvert");
    webcam_src_bin->bin = gst_bin_new("webcam-source-bin");

    // Set file source location
    g_object_set(G_OBJECT(webcam_src_bin->source), "device", device.c_str(), NULL);
    GstCaps *caps = gst_caps_new_simple("image/jpeg",
                               "width", G_TYPE_INT, 1280,
                               "height", G_TYPE_INT, 720,
                               "framerate", GST_TYPE_FRACTION, 25, 1,
                               NULL);
    g_object_set(webcam_src_bin->capsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);

    // Add elements to the bin
    gst_bin_add_many(GST_BIN(webcam_src_bin->bin),
                     GST_ELEMENT(webcam_src_bin->source),
                     GST_ELEMENT(webcam_src_bin->capsfilter),
                     GST_ELEMENT(webcam_src_bin->decoder),
                     GST_ELEMENT(webcam_src_bin->nvvideoconvert),
                     NULL);

    // Link elements
    if (!gst_element_link_many(webcam_src_bin->source, webcam_src_bin->capsfilter, webcam_src_bin->decoder, webcam_src_bin->nvvideoconvert, NULL))
    {
        g_printerr("Elements could not be linked. Exiting.\n");
        return;
    }

    // Add file source bin to the parent bin
    gst_bin_add(GST_BIN(src_bin->bin), webcam_src_bin->bin);

    // Create and add ghost pad
    GstPad *srcpad_nvvideoconvert = gst_element_get_static_pad(webcam_src_bin->nvvideoconvert, "src");
    GstPad *srcpad_file_bin = gst_ghost_pad_new("src", srcpad_nvvideoconvert);
    gst_pad_set_active(srcpad_file_bin, TRUE);

    if (!gst_element_add_pad(webcam_src_bin->bin, srcpad_file_bin))
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
