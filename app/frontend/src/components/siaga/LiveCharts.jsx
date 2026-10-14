import { useState } from "react";
import { LineChart as LineIcon } from "lucide-react";
import { Panel } from "./Panel";
import { TrendChart } from "./TrendChart";
import { TONE } from "@/lib/siaga";

const TABS = [
  { id: "climate", label: "Suhu & Asap", lines: [{ key: "temp", name: "Suhu (°C)", color: TONE.green }, { key: "dust", name: "Asap (mg/m³)", color: TONE.orange, right: true }] },
  { id: "hum", label: "Kelembapan", lines: [{ key: "hum", name: "Kelembapan (%)", color: TONE.green }] },
  { id: "gas", label: "Gas", lines: [{ key: "gas_v", name: "Gas (V)", color: TONE.orange }, { key: "gas_rs", name: "Rs (kΩ)", color: TONE.green, right: true }] },
  { id: "rms", label: "Akustik", lines: [{ key: "rms", name: "RMS", color: TONE.orange }] },
];

export const LiveCharts = ({ series }) => {
  const [tab, setTab] = useState("climate");
  const t = TABS.find((x) => x.id === tab);
  const legend = (
    <div className="hidden md:flex items-center gap-4 mr-2">
      {t.lines.map((l) => (
        <span key={l.key} className="flex items-center gap-2 text-xs font-semibold text-slate-600">
          <span className="w-4 h-[3px] rounded-full" style={{ background: l.color }} />{l.name}
        </span>
      ))}
    </div>
  );
  return (
    <Panel id="sensor" title="Grafik Telemetri Langsung" subtitle={`${series.length} sampel terakhir dari node`} icon={LineIcon} testId="live-charts-panel" right={legend}>
      <div className="inline-flex p-1 rounded-xl bg-slate-100 mb-5" role="tablist">
        {TABS.map((x) => (
          <button key={x.id} data-testid={`chart-tab-${x.id}`} onClick={() => setTab(x.id)} role="tab" aria-selected={tab === x.id}
            className={`px-3.5 py-1.5 text-xs font-semibold rounded-lg transition-colors duration-200 ${tab === x.id ? "bg-white text-emerald-800 shadow-sm" : "text-slate-500 hover:text-slate-800"}`}>
            {x.label}
          </button>
        ))}
      </div>
      {series.length === 0 ? (
        <div className="h-[260px] grid place-items-center text-sm text-slate-400 border border-dashed border-slate-200 rounded-xl">
          Menunggu aliran telemetri dari node…
        </div>
      ) : (
        <TrendChart data={series} lines={t.lines} testId={`live-chart-${t.id}`} />
      )}
    </Panel>
  );
};
