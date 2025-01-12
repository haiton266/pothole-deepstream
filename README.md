```
GST_DEBUG_DUMP_DOT_DIR=/home/jetson/hai/_my-app/debug GST_DEBUG=1 ./build/main2 configs/pipeline_config.txt
```

```
dot -Tpng debug/pipeline.dot -o debug/pipeline.png
dot -Tpng debug/bin.dot -o debug/bin.png
```