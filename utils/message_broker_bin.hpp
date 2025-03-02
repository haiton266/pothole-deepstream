#ifndef GST_MESSAGE_BROKER
#define GST_MESSAGE_BROKER

#include <gst/gst.h>
#include "main.hpp"

// Function to set up the file source bin
void setup_message_broker(MessageBroker *msg_broker_bin, SinkBin *sink_bin);

#endif // GST_MESSAGE_BROKER
