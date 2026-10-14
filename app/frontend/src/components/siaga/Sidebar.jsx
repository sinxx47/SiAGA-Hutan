import { useState } from "react";
import { LayoutGrid, Map, TriangleAlert, BarChart3, Video, Database, Layers, Cpu } from "lucide-react";

export const NAV = [
  { id: "dashboard", label: "Dashboard", icon: LayoutGrid },
  { id: "peta", label: "Peta Pemantauan", icon: Map },
  { id: "peringatan", label: "Peringatan", icon: TriangleAlert },
  { id: "sensor", label: "Data Sensor", icon: BarChart3 },
  { id: "kamera", label: "Kamera", icon: Video },
  { id: "riwayat", label: "Riwayat Data", icon: Database },
];

export const Sidebar = ({ latest, online }) => {
  const [active, setActive] = useState("dashboard");
  const go = (id) => {
    setActive(id);
    document.getElementById(id)?.scrollIntoView({ behavior: "smooth", block: "start" });
  };
  return (
    <aside data-testid="sidebar" className="hidden lg:flex fixed inset-y-0 left-0 w-[250px] bg-[#0B3B2C] text-emerald-50 flex-col z-30">
      <div className="flex items-center gap-3 px-6 h-[84px] border-b border-white/10">
        <span className="w-10 h-10 rounded-xl bg-emerald-500 grid place-items-center shadow-[0_6px_16px_-4px_rgba(16,185,129,0.6)]">
          <Layers className="w-5 h-5 text-white" strokeWidth={2.2} />
        </span>
        <div>
          <p data-testid="app-title" className="font-extrabold text-[17px] leading-tight text-white">SiAGA Hutan</p>
          <p className="text-[11px] text-emerald-300/90 font-medium">Forest Sentinel Node</p>
        </div>
      </div>
      <nav className="flex-1 px-4 py-6 space-y-1">
        {NAV.map(({ id, label, icon: Icon }) => (
          <button key={id} data-testid={`nav-${id}`} onClick={() => go(id)}
            className={`w-full flex items-center gap-3 px-4 py-2.5 rounded-xl text-sm font-semibold transition-colors duration-200 ${active === id ? "bg-white text-[#0B3B2C] shadow-sm" : "text-emerald-100/75 hover:bg-white/5 hover:text-white"}`}>
            <Icon className="w-[18px] h-[18px]" strokeWidth={1.9} />{label}
          </button>
        ))}
      </nav>
      <div className="m-4 p-4 rounded-xl bg-white/5 border border-white/10" data-testid="sidebar-node-card">
        <div className="flex items-center gap-2 text-xs font-semibold text-white">
          <Cpu className="w-4 h-4 text-emerald-300" /> Node Edge
        </div>
        <p className="mono text-[11px] text-emerald-100/70 mt-2 truncate">{latest?.device || "—"}</p>
        <div className="flex items-center gap-2 mt-2 text-[11px] font-semibold">
          <span className={`w-2 h-2 rounded-full ${online ? "bg-emerald-400" : "bg-orange-400"}`} />
          <span className={online ? "text-emerald-300" : "text-orange-300"}>{online ? "Online" : "Offline / Menunggu"}</span>
        </div>
      </div>
    </aside>
  );
};
