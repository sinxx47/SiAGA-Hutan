import json
import logging
import os
import ssl
import uuid
from datetime import datetime, timezone
from pathlib import Path

import httpx
import paho.mqtt.client as mqtt
from dotenv import load_dotenv
from fastapi import APIRouter, FastAPI, HTTPException, Query
from starlette.middleware.cors import CORSMiddleware

load_dotenv(Path(__file__).parent / ".env")

logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(name)s - %(levelname)s - %(message)s")
logger = logging.getLogger("siaga")

SUPABASE_URL = os.environ["SUPABASE_URL"].rstrip("/")
SUPABASE_KEY = os.environ["SUPABASE_KEY"]
SB_HEADERS = {"apikey": SUPABASE_KEY, "Authorization": f"Bearer {SUPABASE_KEY}", "Content-Type": "application/json"}

TOPIC_TELEMETRY = "iot/siaga_hutan/telemetry"
TOPIC_ALERT = "iot/siaga_hutan/alert"
TOPIC_STATUS = "iot/siaga_hutan/status"

state = {"mqtt_connected": False, "last_message_at": None, "last_db_error": None, "inserted": 0}
sb = httpx.Client(base_url=f"{SUPABASE_URL}/rest/v1", headers=SB_HEADERS, timeout=10)


def sb_insert(table: str, row: dict):
    try:
        r = sb.post(f"/{table}", json=row, headers={"Prefer": "return=minimal"})
        if r.status_code >= 300:
            state["last_db_error"] = r.text
            logger.warning("Supabase insert %s failed: %s", table, r.text)
        else:
            state["inserted"] += 1
            state["last_db_error"] = None
    except httpx.HTTPError as e:
        state["last_db_error"] = str(e)
        logger.warning("Supabase error: %s", e)


def parse_payload(raw: bytes):
    text = raw.decode("utf-8", errors="replace").strip()
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return text


def telemetry_row(d: dict) -> dict:
    return {
        "device": d.get("device"),
        "seq": d.get("seq"),
        "temp": d.get("temp"),
        "hum": d.get("hum"),
        "dust": d.get("dust"),
        "gas_v": d.get("gas_v"),
        "gas_rs": d.get("gas_rs"),
        "rms": d.get("rms"),
        "risk": d.get("risk"),
        "servo": d.get("servo"),
        "fan_speed": d.get("fanSpeed"),
        "cam_ack": d.get("camAck"),
        "raw": d,
    }


def alert_row(kind: str, payload) -> dict:
    if isinstance(payload, dict):
        level = payload.get("level", payload.get("risk"))
        msg = payload.get("message") or payload.get("msg") or payload.get("type") or payload.get("status") or json.dumps(payload)
        return {"device": payload.get("device"), "kind": kind, "level": level if isinstance(level, int) else None,
                "message": str(msg), "raw": payload}
    return {"device": None, "kind": kind, "level": None, "message": str(payload), "raw": {"value": payload}}


def on_connect(client, userdata, flags, reason_code, properties=None):
    ok = not reason_code.is_failure
    state["mqtt_connected"] = ok
    logger.info("MQTT connect: %s", reason_code)
    if ok:
        client.subscribe([(TOPIC_TELEMETRY, 1), (TOPIC_ALERT, 1), (TOPIC_STATUS, 1)])


def on_disconnect(client, userdata, flags, reason_code, properties=None):
    state["mqtt_connected"] = False
    logger.warning("MQTT disconnected: %s", reason_code)


def on_message(client, userdata, msg):
    state["last_message_at"] = datetime.now(timezone.utc).isoformat()
    payload = parse_payload(msg.payload)
    if msg.topic == TOPIC_TELEMETRY and isinstance(payload, dict):
        sb_insert("telemetry", telemetry_row(payload))
    elif msg.topic == TOPIC_ALERT:
        sb_insert("alerts", alert_row("alert", payload))
    elif msg.topic == TOPIC_STATUS and not msg.retain:
        sb_insert("alerts", alert_row("status", payload))


mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"siaga-backend-{uuid.uuid4().hex[:8]}")
mqtt_client.username_pw_set(os.environ["MQTT_USERNAME"], os.environ["MQTT_PASSWORD"])
mqtt_client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
mqtt_client.on_connect = on_connect
mqtt_client.on_disconnect = on_disconnect
mqtt_client.on_message = on_message
mqtt_client.reconnect_delay_set(min_delay=1, max_delay=30)

app = FastAPI()
api = APIRouter(prefix="/api")


@api.get("/")
def root():
    return {"service": "SiAGA Hutan Ingest", "ok": True}


@api.get("/health")
def health():
    return state


def sb_select(table: str, limit: int, since: str | None, extra: dict | None = None):
    params = {"select": "*", "order": "created_at.desc", "limit": str(limit)}
    if since:
        params["created_at"] = f"gte.{since}"
    params.update(extra or {})
    try:
        r = sb.get(f"/{table}", params=params)
    except httpx.HTTPError as e:
        raise HTTPException(424, f"Supabase tidak dapat dihubungi: {e}")
    if r.status_code >= 300:
        body = r.json() if r.headers.get("content-type", "").startswith("application/json") else {}
        if body.get("code") == "PGRST205":
            raise HTTPException(424, "Tabel Supabase belum dibuat. Jalankan /app/supabase_schema.sql di Supabase SQL Editor.")
        raise HTTPException(424, body.get("message", r.text))
    return r.json()


@api.get("/history/telemetry")
def history_telemetry(limit: int = Query(200, ge=1, le=2000), since: str | None = None):
    return sb_select("telemetry", limit, since)


@api.get("/history/alerts")
def history_alerts(limit: int = Query(100, ge=1, le=1000), since: str | None = None):
    return sb_select("alerts", limit, since)


app.include_router(api)
app.add_middleware(
    CORSMiddleware,
    allow_credentials=True,
    allow_origins=os.environ.get("CORS_ORIGINS", "*").split(","),
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.on_event("startup")
def start_mqtt():
    mqtt_client.connect_async(os.environ["MQTT_HOST"], int(os.environ["MQTT_PORT"]), keepalive=30)
    mqtt_client.loop_start()


@app.on_event("shutdown")
def stop_mqtt():
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    sb.close()
