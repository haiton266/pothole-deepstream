#ifndef GST_WEBCAM_SRC_BIN_HPP
#define GST_WEBCAM_SRC_BIN_HPP

#include <gst/gst.h>
#include <string>
#include "main.hpp"

// Structure to hold file source bin components
class WebcamSrcBin : public SourceBinParent
{
public:
    GstElement *bin;
    GstElement *source;
    GstElement *capsfilter;
    GstElement *decoder;
    GstElement *nvvideoconvert;
};

// Function to set up the file source bin
void setup_webcam_src_bin(WebcamSrcBin *webcam_src_bin, SrcBin *src_bin, const std::string &device);

#endif // GST_WEBCAM_SRC_BIN_HPP
