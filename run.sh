rm -r ./images_saved/*
rm -r ./debug-payload/*
GST_DEBUG_DUMP_DOT_DIR=/home/jetson/hai/app_copy/_my-app/debug  GST_DEBUG=1 ./build/main2 configs/pipeline_config.txt
