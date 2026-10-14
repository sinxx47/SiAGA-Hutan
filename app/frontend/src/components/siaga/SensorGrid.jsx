import { Thermometer, Droplet, Wind, FlaskConical, Activity, AudioLines } from "lucide-react";
import { SENSORS, TONE, fmt } from "@/lib/siaga";

const ICONS = { temp: Thermometer, hum: Droplet, dust: Wind, gas_v: FlaskConical, gas_rs: Activity, rms: AudioLines };

const StatCard = ({ s, v, index }) => {
  const Icon = ICONS[s.id];
  const color = TONE[s.tone];
  const pct = typeof v === "number" ? Math.min(100, Math.max(0, (v / s.max) * 100)) : 0;
  return (
    <div data-testid={`sensor-card-${s.id}`} className="card-pro rise p-5 hover:-translate-y-0.5" style={{ animationDelay: `${index * 40}ms` }}>
      <div className="flex items-center justify-between">
        <span className="text-sm font-semibold text-slate-600">{s.label}</span>
        <span className="w-8 h-8 rounded-full grid place-items-center" style={{ background: color }}>
          <Icon className="w-4 h-4 text-white" strokeWidth={2.2} />
        </span>
      </div>
      <div className="mt-3 flex items-baseline gap-1">
        <span data-testid={`sensor-value-${s.id}`} className="num text-[28px] font-extrabold text-slate-900 leading-none">{fmt(v, s.digits)}</span>
        <span className="text-sm font-medium text-slate-500">{s.unit}</span>
      </div>
      <div className="mt-4 h-1.5 rounded-full bg-slate-100 overflow-hidden">
        <div className="h-full rounded-full transition-[width] duration-700 ease-out" style={{ width: `${pct}%`, background: color }} />
      </div>
    </div>
  );
};

export const SensorGrid = ({ latest }) => (
  <div data-testid="sensor-grid" className="grid grid-cols-2 md:grid-cols-3 2xl:grid-cols-6 gap-4">
    {SENSORS.map((s, i) => <StatCard key={s.id} s={s} v={latest?.[s.id]} index={i} />)}
  </div>
);
