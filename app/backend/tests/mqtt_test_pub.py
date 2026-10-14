"""Background MQTT publisher for frontend testing. Publishes telemetry every 2s and subscribes to command topic."""
import json, os, ssl, sys, time, uuid, threading
import paho.mqtt.client as mqtt
from dotenv import load_dotenv
load_dotenv("/app/backend/.env")

DURATION = int(sys.argv[1]) if len(sys.argv) > 1 else 90
RISK_SEQ = [0, 0, 1, 1, 2, 2, 1, 0]  # cycle risk values to exercise UI

commands_received = []

def on_connect(c, u, f, rc, p=None):
    print(f"[pub] connected rc={rc}", flush=True)
    c.subscribe("iot/siaga_hutan/command", 1)

def on_message(c, u, msg):
    txt = msg.payload.decode("utf-8", "replace")
    commands_received.append(txt)
    print(f"[cmd-recv] {msg.topic}: {txt}", flush=True)
    with open("/tmp/mqtt_commands.log", "a") as f:
        f.write(txt + "\n")

cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"test-pub-{uuid.uuid4().hex[:6]}")
cli.username_pw_set(os.environ["MQTT_USERNAME"], os.environ["MQTT_PASSWORD"])
cli.tls_set(cert_reqs=ssl.CERT_REQUIRED)
cli.on_connect = on_connect
cli.on_message = on_message
cli.connect(os.environ["MQTT_HOST"], int(os.environ["MQTT_PORT"]), 30)
cli.loop_start()
time.sleep(2)

start = time.time()
seq = int(start) % 100000
i = 0
while time.time() - start < DURATION:
    risk = RISK_SEQ[i % len(RISK_SEQ)]
    temp = 28 + risk * 5 + (i % 3) * 0.1
    payload = {
        "device": "SIAGA-TEST-INGEST", "seq": seq + i,
        "temp": round(temp, 2), "hum": 60 + (i % 10), "dust": 10 + risk * 20 + i % 5,
        "gas_v": 1.0 + risk * 0.3, "gas_rs": 9000 - risk * 1500,
        "rms": 0.02 + risk * 0.05, "risk": risk,
        "servo": 45 + (i % 4) * 30, "fanSpeed": 50 + risk * 70, "camAck": i % 2,
    }
    cli.publish("iot/siaga_hutan/telemetry", json.dumps(payload), qos=0)
    i += 1
    time.sleep(2)
    if i >= 60:  # cap total messages
        break

print(f"[pub] published {i} telemetry; commands received: {len(commands_received)}", flush=True)
time.sleep(2)
cli.loop_stop(); cli.disconnect()
