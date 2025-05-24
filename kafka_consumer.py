# -*- coding: utf-8 -*-
import json
from kafka import KafkaConsumer

def consume_kafka():
    # Connect to Kafka and subscribe to 'test_topic'
    consumer = KafkaConsumer(
        'test_topic',
        bootstrap_servers='localhost:9092',  # Change this if your Kafka broker is remote
        auto_offset_reset='earliest',        # 'latest' to read only new messages
        enable_auto_commit=True,
        group_id='simple-consumer-group',
        value_deserializer=lambda m: json.loads(m.decode('utf-8'))
    )

    print("🟢 Listening to Kafka topic 'test_topic'...")

    for msg in consumer:
        print("📩 Message received from Kafka:")
        print(json.dumps(msg.value, indent=2, ensure_ascii=False))

        # Optional: extract image path if needed
        data = msg.value
        objects = data.get("objects", [])
        for obj in objects:
            columns = obj.split('|')
            if len(columns) >= 6:
                image_path = columns[5]
                print("📷 Image path:", image_path)

if __name__ == "__main__":
    consume_kafka()