import { Activity, Layers } from "lucide-react";
import { clock } from "@/lib/siaga";

const CONN = {
  connected: { label: "MQTT Terhubung", dot: "bg-emerald-500", text: "text-emerald-800" },
  connecting: { label: "Menghubungkan…", dot: "bg-slate-400", text: "text-slate-600" },
  reconnecting: { label: "Menyambung ulang", dot: "bg-orange-500", text: "text-orange-700" },
  disconnected: { label: "MQTT Terputus", dot: "bg-red-500", text: "text-red-700" },
};

export const Topbar = ({ conn, connError, online, latest }) => {
  const c = CONN[conn] || CONN.connecting;
  return (
    <header data-testid="dashboard-header" className="rise flex flex-col md:flex-row md:items-center justify-between gap-4">
      <div className="flex items-center gap-3">
        <span className="lg:hidden w-10 h-10 rounded-xl bg-[#0B3B2C] grid place-items-center"><Layers className="w-5 h-5 text-emerald-300" /></span>
        <div>
          <h1 className="text-3xl md:text-4xl font-extrabold tracking-tight text-slate-900">Dashboard</h1>
          <p className="text-sm text-slate-500 mt-1">Pemantauan dini kebakaran hutan &amp; akustik pembalakan liar</p>
        </div>
      </div>
      <div className="flex flex-wrap items-center gap-2">
        <span data-testid="mqtt-status-badge" title={connError || ""}
          className={`inline-flex items-center gap-2 bg-white border border-slate-200 rounded-full px-3.5 py-2 text-xs font-semibold ${c.text}`}>
          <span className={`w-2 h-2 rounded-full ${c.dot}`} />{c.label}
        </span>
        <span data-testid="device-status-badge"
          className={`inline-flex items-center gap-2 bg-white border border-slate-200 rounded-full px-3.5 py-2 text-xs font-semibold ${online ? "text-emerald-800" : "text-orange-700"}`}>
          <span className={`w-2 h-2 rounded-full ${online ? "bg-emerald-500" : "bg-orange-500"}`} />Node {online ? "Online" : "Offline"}
        </span>
        <span data-testid="seq-badge" className="inline-flex items-center gap-2 bg-white border border-slate-200 rounded-full px-3.5 py-2 text-xs font-semibold text-slate-600">
          <span className="mono">SEQ #{latest?.seq ?? "—"}</span>
          <span className="text-slate-300">|</span>
          <span data-testid="last-update" className="mono">{latest ? clock(latest.ts) : "--:--:--"}</span>
        </span>
        <span data-testid="mode-badge" className="inline-flex items-center gap-2 bg-[#0B3B2C] text-white rounded-full px-4 py-2 text-xs font-bold">
          <Activity className="w-3.5 h-3.5" /> Mode: Live Telemetry
        </span>
      </div>
    </header>
  );
};
