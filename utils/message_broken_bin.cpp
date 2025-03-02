#include <gst/gst.h>
#include "message_broken_bin.hpp"

void setup_message_broken(MessageBroken *msg_broken_bin, SinkBin *sink_bin)
{
    msg_broken_bin->bin = gst_bin_new("msgbroken-sink-bin");

    msg_broken_bin->queue = gst_element_factory_make("queue", "queue");
    msg_broken_bin->nvmsgconv = gst_element_factory_make("nvmsgconv", "msgconv");
    msg_broken_bin->nvmsgbroker = gst_element_factory_make("nvmsgbroker", "nvmsgbroker");

    g_object_set(G_OBJECT(msg_broken_bin->nvmsgconv),
                 "config", "/home/jetson/hai/hung/DeepStream_7_KafkaMetadata/cpp_pipeline/cfg_msgconv.txt",
                 "payload-type", 1,
                 "comp-id", 2,
                 "debug-payload-dir", "./debug-payload",
                 "msg2p-newapi", 1,
                 "frame-interval", 30,
                 NULL);

    g_object_set(msg_broken_bin->nvmsgbroker,
                 "conn-str", "localhost;9092",
                 "proto-lib", "/opt/nvidia/deepstream/deepstream/lib/libnvds_kafka_proto.so",
                 "topic", "test_topic",
                 "new-api", 1,
                 "sync", 1,
                 NULL);

    gst_bin_add_many(GST_BIN(msg_broken_bin->bin),
                     msg_broken_bin->queue,
                     msg_broken_bin->nvmsgconv,
                     msg_broken_bin->nvmsgbroker,
                     NULL);

    if (!gst_element_link_many(msg_broken_bin->queue, msg_broken_bin->nvmsgconv, msg_broken_bin->nvmsgbroker, NULL))
    {
        g_printerr("Elements could not be linked. Exiting.");
        return;
    }

    gst_bin_add(GST_BIN(sink_bin->bin), msg_broken_bin->bin);

    GstPad *sinkpad_queue =
        gst_element_get_static_pad(msg_broken_bin->queue, "sink");
    GstPad *sinkpad_file_bin = gst_ghost_pad_new("sink", sinkpad_queue);
    gst_pad_set_active(sinkpad_file_bin, TRUE);
    if (!gst_element_add_pad(msg_broken_bin->bin, sinkpad_file_bin))
    {
        g_printerr("Failed to add ghost pad in file sink bin\n");
        return;
    }

    // Request a pad from tee
    GstPad *srcpad0 = gst_element_get_request_pad(sink_bin->tee, "src_1"); // Todo: auto increment

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
    g_print("Message broken node initialized\n");
    // Clean up references
    gst_object_unref(srcpad0);
}
