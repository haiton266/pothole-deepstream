# -*- coding: utf-8 -*-
import threading
import json
from kafka import KafkaConsumer
from get_gps import initialize_gps, get_gps_data
from push_rest_server import send_pothole_detection

def consume_kafka():
    consumer = KafkaConsumer('test_topic')
    gps_device = initialize_gps()  # Khởi tạo GPS một lần

    for msg in consumer:
        data = json.loads(msg.value.decode('utf-8'))  # Giải mã JSON
        objects = data.get("objects", [])  # Lấy danh sách objects

        path_image = None
        for obj in objects:
            columns = obj.split('|')
            if len(columns) >= 6 and columns[5][0] == '/':
                path_image = columns[5]

        if path_image:
            print("Received path image:", path_image)

            # Chạy lấy GPS và gửi API trong một luồng riêng
            threading.Thread(target=process_data, args=(gps_device, path_image), daemon=True).start()

def process_data(gps_device, path_image):
    """Lấy GPS và gửi API mà không chặn Kafka Consumer"""
    latitude, longitude = get_gps_data(gps_device)
    response = send_pothole_detection(latitude=latitude, longitude=longitude, image_path=path_image)
    print("API Response:", response)

if __name__ == "__main__":
    consume_kafka()
