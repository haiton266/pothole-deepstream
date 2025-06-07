#!/bin/bash
for file in ./collect/*; do
  if [[ -f "$file" ]]; then
    echo "----------------------------"
    echo "File: $file"
    ffprobe -v error \
      -select_streams v:0 \
      -show_entries format=filename,size,duration,bit_rate \
      -show_entries stream=codec_name,width,height \
      -of default=noprint_wrappers=1:nokey=0 \
      "$file"
  fi
done
