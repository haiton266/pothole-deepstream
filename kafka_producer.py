from kafka import KafkaProducer
import json
import time

def produce_messages():
    producer = KafkaProducer(
        bootstrap_servers='localhost:9092',
        value_serializer=lambda v: json.dumps(v).encode('utf-8')
    )

    # Example messages to send
    messages = [
        {
            "objects": ["obj|50|60|200|220|/path/to/image1.jpg"]
        },
        {
            "objects": ["obj|120|130|350|360|/path/to/image2.jpg"]
        }
    ]

    for msg in messages:
        producer.send('test_topic', msg)
        print(f"Sent message: {msg}")
        producer.flush()  # Make sure message is sent
        time.sleep(1)     # Pause 1 second between messages

    producer.close()

if __name__ == "__main__":
    produce_messages()
