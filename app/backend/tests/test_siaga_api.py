"""Backend API tests for SiAGA Hutan."""
import os
import time
import pytest
import requests

BASE_URL = os.environ.get("REACT_APP_BACKEND_URL", "https://fire-guard-hutan.preview.emergentagent.com").rstrip("/")


@pytest.fixture(scope="module")
def client():
    s = requests.Session()
    s.headers.update({"Content-Type": "application/json"})
    return s


def test_root(client):
    r = client.get(f"{BASE_URL}/api/")
    assert r.status_code == 200
    data = r.json()
    assert data.get("ok") is True
    assert "SiAGA" in data.get("service", "")


def test_health_mqtt_connected(client):
    r = client.get(f"{BASE_URL}/api/health")
    assert r.status_code == 200
    data = r.json()
    assert "mqtt_connected" in data
    assert data["mqtt_connected"] is True, f"Backend MQTT not connected: {data}"


def test_history_telemetry_ok(client):
    r = client.get(f"{BASE_URL}/api/history/telemetry", params={"limit": 5})
    assert r.status_code == 200, r.text
    data = r.json()
    assert isinstance(data, list)


def test_history_alerts_ok(client):
    r = client.get(f"{BASE_URL}/api/history/alerts", params={"limit": 5})
    assert r.status_code == 200, r.text
    assert isinstance(r.json(), list)


def test_history_limit_validation(client):
    r = client.get(f"{BASE_URL}/api/history/telemetry", params={"limit": 0})
    assert r.status_code == 422


def test_telemetry_ingest_after_publish(client):
    """Publish a test telemetry message via MQTT TLS and verify it reaches Supabase through backend."""
    import ssl
    import json
    import uuid
    import paho.mqtt.client as mqtt
    from dotenv import load_dotenv
    from pathlib import Path

    load_dotenv(Path("/app/backend/.env"))

    seq_marker = int(time.time()) % 1000000
    payload = {
        "device": "SIAGA-TEST-INGEST",
        "seq": seq_marker,
        "temp": 29.5, "hum": 65.0, "dust": 12.3,
        "gas_v": 1.2, "gas_rs": 8000, "rms": 0.03,
        "risk": 0, "servo": 90, "fanSpeed": 128, "camAck": 0,
    }
    cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"pytest-{uuid.uuid4().hex[:6]}")
    cli.username_pw_set(os.environ["MQTT_USERNAME"], os.environ["MQTT_PASSWORD"])
    cli.tls_set(cert_reqs=ssl.CERT_REQUIRED)
    cli.connect(os.environ["MQTT_HOST"], int(os.environ["MQTT_PORT"]), 30)
    cli.loop_start()
    time.sleep(1.5)
    info = cli.publish("iot/siaga_hutan/telemetry", json.dumps(payload), qos=1)
    info.wait_for_publish(timeout=5)
    time.sleep(3.0)
    cli.loop_stop()
    cli.disconnect()

    r = client.get(f"{BASE_URL}/api/history/telemetry", params={"limit": 20})
    assert r.status_code == 200
    rows = r.json()
    matched = [row for row in rows if row.get("device") == "SIAGA-TEST-INGEST" and row.get("seq") == seq_marker]
    assert matched, f"Published telemetry (seq={seq_marker}) not found in history. First rows: {rows[:3]}"


def test_alert_ingest_after_publish(client):
    import ssl
    import json
    import uuid
    import paho.mqtt.client as mqtt
    from dotenv import load_dotenv
    from pathlib import Path

    load_dotenv(Path("/app/backend/.env"))
    marker = f"TEST_ALERT_{int(time.time())}"
    cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"pytest-a-{uuid.uuid4().hex[:6]}")
    cli.username_pw_set(os.environ["MQTT_USERNAME"], os.environ["MQTT_PASSWORD"])
    cli.tls_set(cert_reqs=ssl.CERT_REQUIRED)
    cli.connect(os.environ["MQTT_HOST"], int(os.environ["MQTT_PORT"]), 30)
    cli.loop_start()
    time.sleep(1.5)
    payload = {"device": "SIAGA-TEST-INGEST", "level": 2, "message": marker, "type": "fire"}
    cli.publish("iot/siaga_hutan/alert", json.dumps(payload), qos=1).wait_for_publish(5)
    time.sleep(3.0)
    cli.loop_stop(); cli.disconnect()

    r = client.get(f"{BASE_URL}/api/history/alerts", params={"limit": 20})
    assert r.status_code == 200
    rows = r.json()
    assert any(row.get("message") == marker for row in rows), f"Alert not persisted: {rows[:3]}"
