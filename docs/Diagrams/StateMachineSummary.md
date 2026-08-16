State machine (logic only, no LED behavior):

Idle
Meaning: Device is connected to WiFi and MQTT, waiting for next publish cycle.
On timer tick:
If WiFi is down -> go to ConnectingWiFi
Else if MQTT is down -> go to ConnectingMqtt
Else -> go to Publishing
ConnectingWiFi
Meaning: WiFi is not connected yet.
On WiFi got IP event -> mark WiFi connected, move to ConnectingMqtt
While disconnected -> stay in ConnectingWiFi
ConnectingMqtt
Meaning: WiFi is up, MQTT is not ready yet.
On reconnect request conditions met (not already connecting, retry interval passed) -> call MQTT connect
On MQTT connected event -> go to Idle
On WiFi lost -> go to ConnectingWiFi
On MQTT disconnect -> stay/return to ConnectingMqtt (after backoff logic)
Publishing
Meaning: Read sensor values and send one MQTT message.
Actions:
Read temperature, humidity, quality
Build JSON payload
Publish to MQTT topic
After publish attempt -> go to Idle