import { Camera, ShieldCheck, TriangleAlert, Flame, Clock, CheckCircle2, CircleDashed } from "lucide-react";
import { Chip } from "./Panel";
import { riskMeta, clock } from "@/lib/siaga";

const ICON = { safe: ShieldCheck, warn: TriangleAlert, crit: Flame, unknown: Clock };

export const AlertStatus = ({ latest, onSend, disabled }) => {
  const m = riskMeta(latest?.risk);
  const Icon = ICON[m.key];
  const ack = latest?.camAck;
  return (
    <section id="kamera" data-testid="risk-banner-hero" className="card-pro rise p-5 md:p-6 flex flex-col gap-5 scroll-mt-24"
      style={latest?.risk === 2 ? { borderColor: "#FCA5A5", boxShadow: "0 0 0 4px rgba(220,38,38,0.08)" } : {}}>
      <div>
        <div className="flex items-center justify-between">
          <h3 className="text-[15px] font-bold text-slate-900">Status Peringatan</h3>
          <span className="text-xs text-slate-500" data-testid="risk-level-label">{m.level}</span>
        </div>
        <div className="flex items-center gap-3 mt-3">
          <span className="w-11 h-11 rounded-xl grid place-items-center" style={{ background: `${m.color}14` }}>
            <Icon className={`w-5 h-5 ${m.key === "crit" ? "animate-pulse" : ""}`} style={{ color: m.color }} strokeWidth={2} />
          </span>
          <Chip solid color={m.color} testId="risk-status-text">{m.label}</Chip>
        </div>
        <p className="text-sm text-slate-600 leading-relaxed mt-3">{m.desc}</p>
      </div>
      <div data-testid="camera-panel">
        <div className="flex items-center justify-between mb-2.5">
          <span className="text-sm font-semibold text-slate-700">Camera Snapshot</span>
          <button data-testid="camera-snapshot-button" disabled={disabled}
            onClick={() => onSend({ cmd: "capture" }, "Perintah capture dikirim ke ESP32-CAM")}
            className="inline-flex items-center gap-1.5 text-xs font-bold text-slate-700 border border-slate-200 rounded-lg px-3 py-1.5 hover:bg-emerald-50 hover:border-emerald-300 hover:text-emerald-800 active:scale-95 transition-[background-color,border-color,color,transform] duration-150 disabled:opacity-40 disabled:pointer-events-none">
            <Camera className="w-3.5 h-3.5" /> SNAP
          </button>
        </div>
        <div data-testid="camera-viewfinder" className="relative aspect-[16/9] rounded-xl overflow-hidden bg-gradient-to-br from-emerald-50 to-slate-100 border border-slate-200 grid place-items-center">
          <div className="text-center">
            <Camera className="w-8 h-8 text-emerald-700/40 mx-auto" strokeWidth={1.5} />
            <p className="text-xs text-slate-500 mt-2">Snapshot dikirim langsung oleh node ESP32-CAM</p>
          </div>
          <div className="absolute inset-x-0 bottom-0 flex items-center justify-between px-3 py-2 bg-slate-900/70 text-white mono text-[10px]">
            <span className="flex items-center gap-1.5"><span className="w-1.5 h-1.5 rounded-full bg-red-500" />CAM-NODE-01 · PAN {latest?.servo ?? "—"}°</span>
            <span>{latest ? clock(latest.ts) : "--:--:--"}</span>
          </div>
        </div>
        <p data-testid="cam-ack-indicator" className={`mt-2.5 text-xs font-semibold inline-flex items-center gap-1.5 ${ack === true ? "text-emerald-700" : ack === false ? "text-orange-600" : "text-slate-400"}`}>
          {ack === true ? <CheckCircle2 className="w-3.5 h-3.5" /> : <CircleDashed className="w-3.5 h-3.5" />}
          {ack === true ? "Kamera mengonfirmasi (ACK)" : ack === false ? "Menunggu ACK kamera" : "Status ACK tidak dilaporkan"}
        </p>
      </div>
    </section>
  );
};
