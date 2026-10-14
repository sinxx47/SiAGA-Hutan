export const TOPICS = {
  telemetry: "iot/siaga_hutan/telemetry",
  alert: "iot/siaga_hutan/alert",
  command: "iot/siaga_hutan/command",
  status: "iot/siaga_hutan/status",
  trigger_cam: "TRIGGER_CAM",
};

export const RISK = {
  0: { key: "safe", label: "AMAN", level: "Tingkat Risiko 0", color: "#15803D",
    desc: "Kondisi ekosistem hutan stabil. Tidak terdeteksi indikasi asap, gas berbahaya, atau akustik gergaji mesin." },
  1: { key: "warn", label: "WASPADA", level: "Tingkat Risiko 1", color: "#EA580C",
    desc: "Terdeteksi lonjakan partikel debu/asap atau fluktuasi gas. Tingkatkan pemantauan dan siagakan tim patroli." },
  2: { key: "crit", label: "BAHAYA", level: "Tingkat Risiko 2", color: "#DC2626",
    desc: "Indikasi kebakaran kritis! Kipas pemadam dan pemindaian servo aktif. Segera kerahkan tim pemadam." },
};

export const riskMeta = (r) => RISK[r] ?? { key: "unknown", label: "MENUNGGU", level: "Tingkat Risiko —", color: "#64748B",
  desc: "Belum ada telemetri yang diterima dari node edge. Pastikan perangkat menyala dan terhubung ke broker." };

export const SENSORS = [
  { id: "temp", label: "Suhu", unit: "°C", digits: 1, tone: "orange", max: 60 },
  { id: "hum", label: "Kelembapan", unit: "%", digits: 1, tone: "green", max: 100 },
  { id: "dust", label: "Asap / Debu", unit: "mg/m³", digits: 3, tone: "orange", max: 0.5 },
  { id: "gas_v", label: "Gas MQ-2", unit: "V", digits: 2, tone: "orange", max: 3.3 },
  { id: "gas_rs", label: "Resistansi Gas", unit: "kΩ", digits: 1, tone: "green", max: 1000 },
  { id: "rms", label: "Akustik RMS", unit: "rms", digits: 0, tone: "green", max: 4000 },
];

export const TONE = { green: "#15803D", orange: "#EA580C", red: "#DC2626", amber: "#D97706" };

export const NODE_LOCATION = { lat: -7.7725, lng: 112.5380, name: "Pos Pantau Tahura Raden Soerjo, Malang" };

export const fmt = (v, d = 2) => (typeof v === "number" && !Number.isNaN(v) ? v.toFixed(d) : "—");

export const clock = (t) => new Date(t).toLocaleTimeString("id-ID", { hour12: false });

export const stamp = (t) =>
  new Date(t).toLocaleString("id-ID", { hour12: false, day: "2-digit", month: "short", hour: "2-digit", minute: "2-digit", second: "2-digit" });

export const parseJson = (text) => {
  try {
    return JSON.parse(text);
  } catch {
    return text;
  }
};
