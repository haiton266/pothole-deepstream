# Deepstream YOLOv8

This repo is used for DUT science research contest. Using Deepstream and target highest performance on Jetson Nano.

## Installation

Refer for setting up clone by ubuntu: https://www.geeksforgeeks.org/how-to-clone-a-repository-from-gitlab/

```bash
git clone git@github.com:haiton266/deepstream-project.git
```

Install extra
```bash
sudo apt update
sudo apt install libyaml-cpp-dev
sudo apt-get install libgstrtspserver-1.0
```

If you install ok, I comment line 1 in CmakeLists.txt else need clone and build each time

Modify DS version in CmakeLists.txt

## Usage

For the first time, you need create build/
```bash
mkdir build
```

Build and run
```bash
cd build
cmake ..
make -j
```

For the next times, only delete old make
```bash
make clean
```

Running with save dot file and show error
```bash
GST_DEBUG_DUMP_DOT_DIR=/home/jetson/hai/_my-app/debug GST_DEBUG=1 ./build/main2 configs/pipeline_config.txt
```

Debug by exporting pipeline image (make sure install graphviz)
```bash
dot -Tpng debug/pipeline.dot -o debug/pipeline.png
dot -Tpng debug/bin.dot -o debug/bin.png
```

---
#### Overview
The `kafka_consumer.py` script listens for new pothole detection events from a Kafka topic. Upon receiving an event, it:
1. Retrieves GPS coordinates.
2. Reads the associated pothole image.
3. Sends the data to a cloud server via a REST API for map updates.

#### File Location
```
server_local/gps_module/python3/kafka_consumer.py
```

#### Usage
To run the script, execute:
```bash
sudo python3 kafka_consumer.py
```
Note: this code create new thread when subcribe a new pothole
