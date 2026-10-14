import { useCallback, useEffect, useState } from "react";
import axios from "axios";
import { Database, RefreshCw, AlertOctagon } from "lucide-react";
import { Panel, Chip } from "./Panel";
import { TrendChart } from "./TrendChart";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { fmt, riskMeta, stamp, TONE } from "@/lib/siaga";

const API = `${process.env.REACT_APP_BACKEND_URL}/api`;
const RANGES = { "1h": 1, "6h": 6, "24h": 24, "168h": 168 };

const Risk = ({ r }) => { const m = riskMeta(r); return <Chip color={m.color}>{r ?? "—"}</Chip>; };
const Th = ({ children }) => <th className="text-left font-semibold px-4 py-3 text-xs text-slate-500 whitespace-nowrap">{children}</th>;
const Td = ({ children }) => <td className="px-4 py-2.5 text-[13px] text-slate-700 whitespace-nowrap num">{children}</td>;
const Row = ({ children }) => <tr className="border-t border-slate-100 hover:bg-emerald-50/40 transition-colors duration-150">{children}</tr>;

const TelemetryTable = ({ rows }) => (
  <table className="w-full" data-testid="telemetry-history-table">
    <thead className="sticky top-0 bg-slate-50"><tr><Th>Waktu</Th><Th>Seq</Th><Th>Suhu</Th><Th>RH</Th><Th>Asap</Th><Th>Gas V</Th><Th>Rs</Th><Th>RMS</Th><Th>Risiko</Th><Th>Servo</Th><Th>Kipas</Th></tr></thead>
    <tbody>
      {rows.map((r) => (
        <Row key={r.id}><Td>{stamp(r.created_at)}</Td><Td>{r.seq}</Td><Td>{fmt(r.temp)}</Td><Td>{fmt(r.hum)}</Td><Td>{fmt(r.dust, 3)}</Td>
          <Td>{fmt(r.gas_v)}</Td><Td>{fmt(r.gas_rs, 1)}</Td><Td>{fmt(r.rms, 0)}</Td><Td><Risk r={r.risk} /></Td><Td>{r.servo ?? "—"}</Td><Td>{r.fan_speed ?? "—"}</Td></Row>
      ))}
    </tbody>
  </table>
);

const AlertTable = ({ rows }) => (
  <table className="w-full" data-testid="alert-history-table">
    <thead className="sticky top-0 bg-slate-50"><tr><Th>Waktu</Th><Th>Jenis</Th><Th>Level</Th><Th>Perangkat</Th><Th>Pesan</Th></tr></thead>
    <tbody>
      {rows.map((r) => (
        <Row key={r.id}><Td>{stamp(r.created_at)}</Td><Td>{r.kind}</Td><Td><Risk r={r.level} /></Td><Td>{r.device || "—"}</Td><Td>{r.message}</Td></Row>
      ))}
    </tbody>
  </table>
);

const Empty = ({ text }) => <p className="text-sm text-slate-400 py-10 text-center">{text}</p>;

const Seg = ({ value, onChange }) => (
  <div className="inline-flex p-1 rounded-xl bg-slate-100">
    {[["telemetry", "Telemetri"], ["alerts", "Peringatan"]].map(([id, l]) => (
      <button key={id} data-testid={`history-tab-${id}`} onClick={() => onChange(id)}
        className={`px-3.5 py-1.5 text-xs font-semibold rounded-lg transition-colors duration-200 ${value === id ? "bg-white text-emerald-800 shadow-sm" : "text-slate-500 hover:text-slate-800"}`}>{l}</button>
    ))}
  </div>
);

export const HistoryPanel = () => {
  const [range, setRange] = useState("1h");
  const [tab, setTab] = useState("telemetry");
  const [tele, setTele] = useState([]);
  const [alerts, setAlerts] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);

  const load = useCallback(async () => {
    setLoading(true);
    const since = new Date(Date.now() - RANGES[range] * 3600 * 1000).toISOString();
    try {
      const [t, a] = await Promise.all([
        axios.get(`${API}/history/telemetry`, { params: { limit: 1000, since } }),
        axios.get(`${API}/history/alerts`, { params: { limit: 300, since } }),
      ]);
      setTele(t.data);
      setAlerts(a.data);
      setError(null);
    } catch (e) {
      setError(e.response?.data?.detail || e.message);
    } finally {
      setLoading(false);
    }
  }, [range]);

  useEffect(() => { load(); }, [load]);

  const chartData = [...tele].reverse().map((r) => ({ ...r, ts: new Date(r.created_at).getTime() }));
  const controls = (
    <>
      <Select value={range} onValueChange={setRange}>
        <SelectTrigger data-testid="history-range-select" className="h-9 w-[110px] text-xs font-semibold bg-white"><SelectValue /></SelectTrigger>
        <SelectContent>
          <SelectItem value="1h" data-testid="range-1h">1 Jam</SelectItem>
          <SelectItem value="6h" data-testid="range-6h">6 Jam</SelectItem>
          <SelectItem value="24h" data-testid="range-24h">24 Jam</SelectItem>
          <SelectItem value="168h" data-testid="range-168h">7 Hari</SelectItem>
        </SelectContent>
      </Select>
      <button data-testid="history-refresh-button" onClick={load} className="h-9 w-9 grid place-items-center rounded-lg border border-slate-200 text-slate-500 hover:text-emerald-700 hover:border-emerald-300 transition-colors duration-200">
        <RefreshCw className={`w-4 h-4 ${loading ? "animate-spin" : ""}`} />
      </button>
    </>
  );

  return (
    <Panel id="riwayat" title="Riwayat Data" subtitle={`Tersimpan di Supabase · ${tele.length} telemetri, ${alerts.length} peringatan`} icon={Database} testId="history-panel" right={controls}>
      {error ? (
        <div data-testid="history-error" className="flex gap-3 items-start rounded-xl bg-orange-50 p-4 text-sm text-orange-800">
          <AlertOctagon className="w-5 h-5 shrink-0 mt-0.5" />
          <div><p className="font-semibold">Riwayat belum tersedia</p><p className="text-xs mt-1 break-all">{error}</p></div>
        </div>
      ) : (
        <>
          <Seg value={tab} onChange={setTab} />
          {tab === "telemetry" ? (
            <div className="mt-5 space-y-5">
              {chartData.length > 1 && (
                <TrendChart data={chartData} testId="history-chart" height={200}
                  lines={[{ key: "temp", name: "Suhu", color: TONE.green }, { key: "dust", name: "Asap", color: TONE.orange, right: true }]} />
              )}
              <div className="max-h-[340px] overflow-auto rounded-xl border border-slate-200">
                {tele.length ? <TelemetryTable rows={tele} /> : <Empty text="Tidak ada data pada rentang ini" />}
              </div>
            </div>
          ) : (
            <div className="mt-5 max-h-[420px] overflow-auto rounded-xl border border-slate-200">
              {alerts.length ? <AlertTable rows={alerts} /> : <Empty text="Tidak ada peringatan" />}
            </div>
          )}
        </>
      )}
    </Panel>
  );
};
