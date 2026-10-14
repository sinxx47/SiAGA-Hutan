import { useCallback, useEffect, useState } from "react";
import { toast } from "sonner";
import { useSiagaMqtt } from "@/hooks/useSiagaMqtt";
import { Sidebar } from "@/components/siaga/Sidebar";
import { Topbar } from "@/components/siaga/Topbar";
import { SensorGrid } from "@/components/siaga/SensorGrid";
import { NodeMap } from "@/components/siaga/NodeMap";
import { AlertStatus } from "@/components/siaga/AlertStatus";
import { LiveCharts } from "@/components/siaga/LiveCharts";
import { EventFeed } from "@/components/siaga/EventFeed";
import { HistoryPanel } from "@/components/siaga/HistoryPanel";
import { TOPICS } from "@/lib/siaga";

const useOnline = (deviceStatus, latest) => {
  const [now, setNow] = useState(Date.now());
  useEffect(() => {
    const t = setInterval(() => setNow(Date.now()), 2000);
    return () => clearInterval(t);
  }, []);
  const fresh = latest && now - latest.ts < 15000;
  const offlineAfter = deviceStatus.status === "offline" && latest && deviceStatus.ts > latest.ts;
  return Boolean(fresh) && !offlineAfter;
};

export default function Dashboard() {
  const { conn, connError, deviceStatus, latest, series, events, sendCommand } = useSiagaMqtt();
  const online = useOnline(deviceStatus, latest);
  const disabled = conn !== "connected";

  const onSend = useCallback(async (cmd, okMsg) => {
    try {
      await sendCommand(cmd);
      toast.success(okMsg, { description: `${TOPICS.command} · ${JSON.stringify(cmd)}` });
    } catch (e) {
      toast.error("Perintah gagal dikirim", { description: e.message });
    }
  }, [sendCommand]);

  return (
    <div className="min-h-screen bg-[#F3F5F1]">
      <Sidebar latest={latest} online={online} />
      <main id="dashboard" data-testid="dashboard-root" className="lg:pl-[250px]">
        <div className="max-w-[1500px] mx-auto px-4 md:px-8 py-6 md:py-8 space-y-6">
          <Topbar conn={conn} connError={connError} online={online} latest={latest} />
          <SensorGrid latest={latest} />
          <div className="grid grid-cols-1 xl:grid-cols-12 gap-6">
            <div className="xl:col-span-8"><NodeMap latest={latest} online={online} /></div>
            <div className="xl:col-span-4"><AlertStatus latest={latest} onSend={onSend} disabled={disabled} /></div>
          </div>
          <LiveCharts series={series} />
          <div className="grid grid-cols-1 xl:grid-cols-12 gap-6">
            <div className="xl:col-span-4"><EventFeed events={events} /></div>
            <div className="xl:col-span-8"><HistoryPanel /></div>
          </div>
          <footer className="text-xs text-slate-400 flex flex-wrap gap-x-6 gap-y-1 pt-2 mono">
            <span>{TOPICS.telemetry}</span><span>{TOPICS.alert}</span><span>{TOPICS.command}</span><span>{TOPICS.status}</span>
          </footer>
        </div>
      </main>
    </div>
  );
}
