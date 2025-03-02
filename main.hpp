#ifndef MAIN_HPP
#define MAIN_HPP

#include <string>

using namespace std;

#define CONFIG_GROUP_TRACKER "tracker"
#define CONFIG_GROUP_TRACKER_WIDTH "tracker-width"
#define CONFIG_GROUP_TRACKER_HEIGHT "tracker-height"
#define CONFIG_GROUP_TRACKER_LL_CONFIG_FILE "ll-config-file"
#define CONFIG_GROUP_TRACKER_LL_LIB_FILE "ll-lib-file"
#define CONFIG_GPU_ID "gpu-id"
#define CHECK_ERROR(error)                                                   \
    if (error)                                                               \
    {                                                                        \
        g_printerr("Error while parsing config file: %s\n", error->message); \
        goto done;                                                           \
    }

// Base class for source bins
class SourceBinParent
{
public:
    virtual ~SourceBinParent() = default;
};

// Base class for sink bins
class SinkBinParent
{
public:
    virtual ~SinkBinParent() = default;
};

class SrcBin
{
public:
    SourceBinParent *source_bin;
    GstElement *bin;
    GstElement *nvstreammux;
    GstPad *ghost_pad;
};

class ProcessBin
{
public:
    GstElement *bin;
    GstElement *nvinfer;
    GstElement *nvtracker;
    GstElement *nvvidconv;
    GstElement *nvdsosd;
    GstPad *src_ghost_pad;
    GstPad *sink_ghost_pad;
};

class SinkBin
{
public:
    GstElement *bin;
    SinkBinParent *sink_bin;
    GstElement *tee;
    GstPad *sink_ghost_pad;
};

class Pipeline
{
public:
    SrcBin *src_bin;
    ProcessBin *process_bin;
    SinkBin *sink_bin;
    GstElement* pipeline;

};

#endif // MAIN_HPP
