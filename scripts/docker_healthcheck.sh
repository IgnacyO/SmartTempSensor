#!/usr/bin/env bash
set -euo pipefail

HOST="${1:-localhost}"

printf '\n[1/5] Starting docker stack...\n'
docker compose up -d

printf '\n[2/5] Checking container status...\n'
docker compose ps

printf '\n[3/5] Checking MQTT port %s:1883...\n' "$HOST"
python - "$HOST" <<'PY'
import socket
import sys

host = sys.argv[1] if len(sys.argv) > 1 else 'localhost'
port = 1883
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(5)
try:
    sock.connect((host, port))
    print(f"[OK] MQTT port is reachable at {host}:{port}")
except Exception as exc:
    print(f"[FAIL] MQTT port unreachable at {host}:{port}: {exc}")
    raise SystemExit(1)
finally:
    sock.close()
PY

printf '\n[4/5] Checking InfluxDB health...\n'
curl -fsS "http://${HOST}:8086/ping"

printf '\n[5/5] Checking Grafana health...\n'
curl -fsS "http://${HOST}:3000/api/health"

printf '\n[PASS] Docker health checks passed.\n'
