import paho.mqtt.client as mqtt
import sys
import time
import json
import random


# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")


# The callback for when a PUBLISH message is sent to the server.
def on_publish(client, userdata, mid, reason_code, properties):
    print(f"Result code {reason_code}")
    print("Published")


mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_publish = on_publish

mqttc.connect(sys.argv[1], 1883, 60)

mqttc.loop_start()

LOCATIONS = ["room/A", "room/B", "room/C", "room/D"]

while True:
    time.sleep(1)
    payload_dict = {
        "sensor": "test-client",
        "temperature": random.random(),
        "humidity": random.random(),
        "quality": 1.0,
    }
    payload_str = json.dumps(payload_dict)
    print(payload_str)

    location = random.randrange(0, len(LOCATIONS))
    mqttc.publish(f"test/{LOCATIONS[location]}", payload=payload_str, qos=0)

mqttc.loop_stop()
