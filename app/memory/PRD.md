# SiAGA Hutan – Forest Sentinel & Acoustic Fire Guard (PRD)

## Original Problem
Mission-critical IoT monitoring & command dashboard (dark tactical UI) for an edge-IoT forest fire / illegal-logging early-warning node (ESP32-CAM, servo pan, fan actuator). MQTT over WSS (HiveMQ Cloud). Topics: iot/siaga_hutan/telemetry, /alert, /command, /status (LWT). Telemetry JSON: device, seq, temp, hum, dust, gas_v, gas_rs, rms, risk, servo, fanSpeed, camAck.

## User Choices
- HiveMQ Cloud (private cluster, creds in env)
- Live MQTT data only (no simulator)
- History in Supabase (REST + anon key; tables via /app/supabase_schema.sql – created by user)
- Commands: servo slider/presets, fan speed, camera capture, reset alarm. Format `{"cmd":"servo|fan|capture|reset_alarm","value":N,"ts":..,"source":"web-dashboard"}` (confirmed by user)
- UI language: Bahasa Indonesia

## Architecture
- Frontend (React): mqtt.js direct WSS (8884) for live data + command publish; Recharts; shadcn
- Backend (FastAPI): paho-mqtt TLS (8883) subscriber → inserts into Supabase `telemetry` & `alerts`; `/api/health`, `/api/history/telemetry`, `/api/history/alerts`

## Implemented (2026-10-12)
- Header with broker/node status (LWT + stale detection), device id, seq
- Risk hero banner (0/1/2, pulsing crimson on critical)
- 6 sensor cards with delta & level bar; live charts tabs
- ESP32-CAM panel (capture, camAck), servo gauge + slider/presets, fan PWM control, reset alarm with confirm
- Live event feed (alerts, risk changes, status, commands)
- Supabase history panel (range filter, telemetry/alert tables, chart)
- Bug fix: insecure WebSocket error → forced wss
- Tested: iteration_1 100% backend/frontend

## 2026-10-14 – Light Professional Redesign
- White base + dark-green sidebar + green/orange accents (Plus Jakarta Sans), section nav
- Sensor stat cards, Leaflet OpenTopoMap node map (fixed default coord in lib/siaga.js NODE_LOCATION), Status Peringatan + Camera Snapshot card, line charts, controls row, event log + history
- Node online logic: fresh telemetry overrides older retained LWT offline
- Tested: iteration_2 100%
- 2026-10-14: Removed Kontrol Aktuator section (servo, fan, reset alarm) + its nav item per user request; SNAP camera button remains

## Backlog
- P1: Live camera snapshot display (needs image URL/topic from firmware)
- P1: Browser notifications / siren sound on risk 2
- P2: Map of node location, multi-node support, CSV export, auth for command access
