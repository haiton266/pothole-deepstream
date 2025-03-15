cd /home/jetson/kafka_2.13-3.9.0

rm -rf /tmp/kafka-logs/

bin/kafka-server-start.sh config/server.properties
