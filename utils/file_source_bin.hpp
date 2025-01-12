#ifndef GST_FILE_SRC_BIN_HPP
#define GST_FILE_SRC_BIN_HPP

#include <gst/gst.h>
#include <string>
#include "main.hpp"

// Structure to hold file source bin components
class FileSrcBin : public SourceBinParent
{
public:
    GstElement *bin;
    GstElement *filesrc;
    GstElement *demuxer;
    GstElement *h264parse;
    GstElement *decoder;
};

// Function to set up the file source bin
void setup_file_src_bin(FileSrcBin *file_src_bin, SrcBin *src_bin, const std::string &location);

#endif // GST_FILE_SRC_BIN_HPP
