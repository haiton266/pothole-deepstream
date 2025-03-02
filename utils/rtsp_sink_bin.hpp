#ifndef GST_RTSP_SINK_BIN_HPP
#define GST_RTSP_SINK_BIN_HPP

#include <gst/gst.h>
#include <string>
#include "main.hpp"

// Structure to hold file source bin components
class RTSPSinkBin : public SinkBinParent
{
public:
    GstElement *bin;
    GstElement *queue;
    GstElement *nvvidconv2;
    GstElement *encoder;
    GstElement *rtph264pay;
    GstElement *udpsink;
};

// Function to set up the file source bin
void setup_rtsp_sink_bin(RTSPSinkBin *rtsp_sink_bin, SinkBin *sink_bin, std::string ip, int port);
#endif // GST_RTSP_SINK_BIN_HPP
