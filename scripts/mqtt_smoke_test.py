#!/usr/bin/env python3
"""Minimal MQTT smoke test for the SmartTempSensor project.

Usage:
    python scripts/mqtt_smoke_test.py --host localhost --topic "test/#" --timeout 20
"""

import argparse
import json
import sys
import time

try:
    import paho.mqtt.client as mqtt
except ImportError as exc:
    raise SystemExit(
        "paho-mqtt is required. Install it with: pip install paho-mqtt"
    ) from exc


received_messages = []


def validate_payload(payload: str):
    try:
        data = json.loads(payload)
    except json.JSONDecodeError as exc:
        raise ValueError(f"Invalid JSON payload: {payload!r} ({exc})") from exc

    required_keys = {"sensor", "temperature", "humidity", "quality"}
    missing = sorted(required_keys - set(data.keys()))
    if missing:
        raise ValueError(f"Missing keys in payload: {missing}")

    if data.get("sensor") in (None, ""):
        raise ValueError("Payload has empty sensor name")

    for key in ("temperature", "humidity", "quality"):
        value = data.get(key)
        if value is None:
            raise ValueError(f"Payload field '{key}' is null")

    return data


def on_connect(client, userdata, flags, reason_code, properties=None):
    client.subscribe(args.topic, qos=0)
    print(f"[MQTT] Subscribed to {args.topic}")


def on_message(client, userdata, msg):
    payload = msg.payload.decode("utf-8", errors="replace")
    print(f"[MQTT] Received on {msg.topic}: {payload}")

    try:
        data = validate_payload(payload)
    except ValueError as exc:
        print(f"[FAIL] Payload validation failed: {exc}")
        return

    received_messages.append(data)
    if len(received_messages) >= 1:
        print("[OK] Payload validation passed")
        client.disconnect()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="MQTT smoke test for SmartTempSensor")
    parser.add_argument("--host", default="localhost", help="MQTT broker host")
    parser.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    parser.add_argument(
        "--topic", default="test/#", help="MQTT topic filter to subscribe to"
    )
    parser.add_argument(
        "--timeout", type=float, default=20.0, help="Seconds to wait before failing"
    )
    args = parser.parse_args()

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        print(f"[MQTT] Connecting to {args.host}:{args.port}...")
        client.connect(args.host, args.port, 60)
        client.loop_start()
        time.sleep(args.timeout)
        client.loop_stop()
    except Exception as exc:
        print(f"[FAIL] MQTT connect failed: {exc}")
        sys.exit(1)

    if not received_messages:
        print(f"[FAIL] No valid MQTT payload received within {args.timeout} seconds")
        sys.exit(1)

    print("[PASS] MQTT smoke test passed")
    sys.exit(0)
