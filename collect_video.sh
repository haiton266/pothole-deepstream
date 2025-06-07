FILENAME="collect/output_$(date +%Y%m%d_%H%M%S).mp4"

gst-launch-1.0 -e v4l2src device=/dev/video0 ! \
  image/jpeg, width=1280, height=720, framerate=25/1 ! \
  jpegdec ! \
  nvvideoconvert ! \
  nvv4l2h264enc ! \
  h264parse ! \
  qtmux ! \
  filesink location=$FILENAME

# gst-launch-1.0 -e filesrc location=/home/jetson/hai/app_copy/_my-app/videos/input/dashcam.mp4 ! \
# qtdemux name=demux demux.video_0 ! queue ! h264parse ! avdec_h264 ! videoconvert ! x264enc ! mp4mux ! \
# filesink location=output.mp4
