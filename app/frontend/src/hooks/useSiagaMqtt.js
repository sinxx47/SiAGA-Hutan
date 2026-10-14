import { useCallback, useEffect, useRef, useState } from "react";
import mqtt from "mqtt";
import { TOPICS, parseJson, riskMeta } from "@/lib/siaga";

const MAX_POINTS = 90;
const MAX_EVENTS = 80;

const eventFrom = (kind, payload, level = null) => {
  const obj = typeof payload === "object" && payload !== null ? payload : null;
  const msg = obj ? obj.message || obj.msg || obj.type || obj.status || JSON.stringify(obj) : String(payload);
  const lvl = level ?? (obj ? obj.level ?? obj.risk ?? null : null);
  return { id: `${Date.now()}-${Math.random()}`, ts: Date.now(), kind, level: lvl, message: String(msg), raw: payload };
};

export function useSiagaMqtt() {
  const [conn, setConn] = useState("connecting");
  const [connError, setConnError] = useState(null);
  const [deviceStatus, setDeviceStatus] = useState({ status: "unknown", ts: null });
  const [latest, setLatest] = useState(null);
  const [series, setSeries] = useState([]);
  const [events, setEvents] = useState([]);
  const clientRef = useRef(null);
  const lastRisk = useRef(null);

  const pushEvent = useCallback((e) => setEvents((prev) => [e, ...prev].slice(0, MAX_EVENTS)), []);

  useEffect(() => {
    const url = (process.env.REACT_APP_MQTT_URL || "").replace(/^wss?:\/\//, "wss://");
    if (!url.startsWith("wss://")) {
      setConn("disconnected");
      setConnError("REACT_APP_MQTT_URL tidak dikonfigurasi");
      return undefined;
    }
    const client = mqtt.connect(url, {
      protocol: "wss",
      username: process.env.REACT_APP_MQTT_USERNAME,
      password: process.env.REACT_APP_MQTT_PASSWORD,
      clientId: `siaga-web-${Math.random().toString(16).slice(2, 10)}`,
      reconnectPeriod: 3000,
      connectTimeout: 12000,
      clean: true,
    });
    clientRef.current = client;

    client.on("connect", () => {
      setConn("connected");
      setConnError(null);
      client.subscribe([TOPICS.telemetry, TOPICS.alert, TOPICS.status], { qos: 1 });
    });
    client.on("reconnect", () => setConn("reconnecting"));
    client.on("close", () => setConn((c) => (c === "connected" ? "reconnecting" : c)));
    client.on("offline", () => setConn("disconnected"));
    client.on("error", (err) => setConnError(err?.message || "Koneksi gagal"));

    client.on("message", (topic, buf) => {
      const payload = parseJson(buf.toString());
      if (topic === TOPICS.telemetry && typeof payload === "object") {
        const now = Date.now();
        setLatest({ ...payload, ts: now });
        setSeries((prev) => [...prev, { ...payload, ts: now }].slice(-MAX_POINTS));
        if (lastRisk.current !== null && payload.risk !== lastRisk.current) {
          pushEvent(eventFrom("risk", `Risiko berubah ${lastRisk.current} → ${payload.risk} (${riskMeta(payload.risk).label})`, payload.risk));
        }
        lastRisk.current = payload.risk;
      } else if (topic === TOPICS.alert) {
        pushEvent(eventFrom("alert", payload));
      } else if (topic === TOPICS.status) {
        const s = typeof payload === "object" ? payload.status : payload;
        setDeviceStatus({ status: String(s || "unknown").toLowerCase(), ts: Date.now(), device: payload?.device });
        pushEvent(eventFrom("status", `Node ${String(s).toUpperCase()}`));
      }
    });

    return () => client.end(true);
  }, [pushEvent]);

  const sendCommand = useCallback(
    (cmd, targetTopic = null) =>
      new Promise((resolve, reject) => {
        const client = clientRef.current;
        if (!client || !client.connected) return reject(new Error("Broker MQTT tidak terhubung"));
        const topic = targetTopic || TOPICS.command;
        const payload = typeof cmd === "string" ? cmd : JSON.stringify({ ...cmd, ts: Date.now(), source: "web-dashboard" });
        client.publish(topic, payload, { qos: 1 }, (err) => {
          if (err) return reject(err);
          pushEvent(eventFrom("command", `CMD [${topic}]: ${payload}`));
          // Jika perintah TRIGGER_CAM dikirim ke topik khusus, publikasikan juga ke TOPICS.command
          // agar kompatibel dengan firmware ESP32-S3 yang memantau TOPIC_CMD
          if (cmd === "TRIGGER_CAM" && topic !== TOPICS.command) {
            client.publish(TOPICS.command, "TRIGGER_CAM", { qos: 1 });
          }
          resolve(payload);
        });
      }),
    [pushEvent]
  );

  return { conn, connError, deviceStatus, latest, series, events, sendCommand };
}
