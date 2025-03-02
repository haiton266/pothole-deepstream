#ifndef GST_MESSAGE_BROKEN
#define GST_MESSAGE_BROKEN

#include <gst/gst.h>
#include "main.hpp"

class MessageBroken : public SinkBinParent
{
public:
    GstElement *bin;
    GstElement *queue;
    GstElement *nvmsgconv;
    GstElement *nvmsgbroker;
};

// Function to set up the file source bin
void setup_message_broken(MessageBroken *msg_broken_bin, SinkBin *sink_bin);

#endif // GST_MESSAGE_BROKEN
