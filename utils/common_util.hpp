#ifndef COMMON_UTIL_HPP
#define COMMON_UTIL_HPP

#include <gst/gst.h>
#include "main.hpp"
gboolean bus_call(G_GNUC_UNUSED GstBus *bus, GstMessage *msg, gpointer data);

gchar *get_absolute_file_path(const gchar *cfg_file_path, const gchar *file_path);

gboolean
set_tracker_properties(GstElement *nvtracker, gchar *tracker_config_file);

#endif // COMMON_UTIL_HPP
