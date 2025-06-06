# -*- coding: utf-8 -*-
# import threading
import json
from kafka import KafkaConsumer
from get_gps import initialize_gps, get_gps_data
from push_rest_server import send_pothole_detection

def consume_kafka():
    gps_device = initialize_gps()  # Initialize GPS once
    consumer = KafkaConsumer('test_topic')

    for msg in consumer:
        data = json.loads(msg.value.decode('utf-8'))  # Decode JSON
        objects = data.get("objects", [])  # Get list of objects
        potholes = []
        path_image = None
        for obj in objects:
            columns = obj.split('|')
            if len(columns) >= 6 and columns[5][1] == '/':
                path_image = columns[5]
            x1, y1, x2, y2 = columns[1:5]
            potholes.append((x1, y1, x2, y2))

        if path_image:
            print("Received path image:", path_image)
            process_data(gps_device, path_image, potholes)

def process_data(gps_device, path_image, potholes):
    """Get GPS and send API without blocking Kafka Consumer"""
    latitude, longitude = get_gps_data(gps_device)
    print("GPS Data:", latitude, longitude)
    response = send_pothole_detection(latitude, longitude, path_image, potholes)
    print("API Response:", response)

if __name__ == "__main__":
    consume_kafka()
