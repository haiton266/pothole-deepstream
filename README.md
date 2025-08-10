# Deepstream YOLOv11

This repo is used for the DUT science research contest. Using DeepStream and targeting the highest performance on Jetson Nano.

Output Results - Frame with ROI
![Output Frame with ROI](assets/img700.jpg)

DeepStream Pipeline (Exported from GST DOT FILE)

<div style="overflow-x: auto; white-space: nowrap; max-height: height;">
  <img src="assets/pipeline.png" alt="DeepStream Pipeline" style="max-width: none; height: 160px;">
</div>

## Installation

Refer for setting up a clone by Ubuntu: https://www.geeksforgeeks.org/how-to-clone-a-repository-from-gitlab/

```bash
git clone git@github.com:haiton266/deepstream-project.git
```

Install extra
```bash
sudo apt update
sudo apt install libyaml-cpp-dev
sudo apt-get install libgstrtspserver-1.0
```

If you install ok, I comment line 1 in CmakeLists.txt otherwise, you need to clone and build each time

Modify DS version in CMakeLists.txt

Debug by exporting pipeline image (make sure to install Graphviz)
```bash
dot -Tpng debug/pipeline.dot -o debug/pipeline.png
dot -Tpng debug/bin.dot -o debug/bin.png
```
---
### Build and Execution Scripts
- **`build.sh`**: Cleans old compiled files and rebuilds the project.
- **`run.sh`**: Executes the main application code.

### Kafka Broker Management
- **`broker1.sh`**: First Initialize Kafka broker.
- **`broker2.sh`**: Second Initialize Kafka broker.
- **`kafka.sh`**: Runs the Kafka consumer. Source in `server_local/gps_module/python3/kafka_consumer.py`
---
The `kafka_consumer.py` script listens for new pothole detection events from a Kafka topic. Upon receiving an event, it:
1. Retrieves GPS coordinates.
2. Reads the associated pothole image.
3. Sends the data to a cloud server via a REST API for map updates.

### 📂 Some useful Ipynb files run by Google Colab related to Data and AI

| Notebook  | Link |
|-----------|------|
| Convert .pt to .onnx model | [![Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/drive/1oDeOanjJrb2F7yEUOOqP42MKH9R3Rv6z) |
| FiftyOne to validate dataset | [![Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/drive/1UqskgxYIuALjBIv_g_aaxTW5DieYYbqE) |
| Process data Kaggle Pothole | [![Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/drive/1yxJNHxrcL7UKLGlrL16cSXgr6nXKtYhW#scrollTo=TAyC6py8K4bx) |
| Test YOLO | [![Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/drive/1YCGI9db6zX54XI6ZZo55jC0nyoXe7uCY?usp=sharing) |
| Youtube_pothole_dataset_upload_roboflow | [![Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/drive/1wsQZyXeLhr-54hV26Nu9JXvd0hgF4faa?usp=sharing) |
