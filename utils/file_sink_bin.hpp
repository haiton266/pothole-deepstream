#ifndef GST_FILE_SINK_BIN_HPP
#define GST_FILE_SINK_BIN_HPP

#include <gst/gst.h>
#include <string>
#include "main.hpp"

// Structure to hold file source bin components
class FileSinkBin : public SinkBinParent
{
public:
    GstElement *bin;
    GstElement *queue;
    GstElement *nvvidconv2;
    GstElement *encoder;
    GstElement *parser;
    GstElement *qtmux;
    GstElement *filesink;
};

// Function to set up the file source bin
void setup_file_sink_bin(FileSinkBin *file_sink_bin, SinkBin *sink_bin, const std::string &location);

#endif // GST_FILE_SINK_BIN_HPP
