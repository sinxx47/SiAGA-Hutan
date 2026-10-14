import { Bell } from "lucide-react";
import { Panel } from "./Panel";
import { clock, TONE } from "@/lib/siaga";

const KIND = {
  alert: { label: "Alert", color: TONE.red },
  risk: { label: "Risiko", color: TONE.orange },
  status: { label: "Status", color: TONE.green },
  command: { label: "Perintah", color: "#0F766E" },
};

export const EventFeed = ({ events }) => (
  <Panel id="peringatan" title="Log Kejadian" subtitle="Alert, perubahan risiko, status & perintah" icon={Bell} testId="event-feed-panel"
    right={<span className="text-xs font-semibold text-slate-500 bg-slate-100 rounded-full px-2.5 py-1">{events.length}</span>}>
    <ul className="divide-y divide-slate-100 max-h-[360px] overflow-y-auto -mx-1 px-1" data-testid="event-feed-list">
      {events.length === 0 && (
        <li className="text-sm text-slate-400 py-10 text-center">Belum ada kejadian</li>
      )}
      {events.map((e) => {
        const k = KIND[e.kind] || KIND.alert;
        return (
          <li key={e.id} data-testid={`event-item-${e.kind}`} className="rise flex gap-3 items-start py-3">
            <span className="w-2 h-2 rounded-full mt-1.5 shrink-0" style={{ background: k.color }} />
            <div className="min-w-0 flex-1">
              <div className="flex items-center gap-2">
                <span className="text-xs font-bold" style={{ color: k.color }}>{k.label}</span>
                <span className="mono text-[11px] text-slate-400">{clock(e.ts)}</span>
              </div>
              <p className="text-[13px] text-slate-700 break-all mt-0.5">{e.message}</p>
            </div>
          </li>
        );
      })}
    </ul>
  </Panel>
);
