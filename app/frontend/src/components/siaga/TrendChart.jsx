import { CartesianGrid, Line, LineChart, ResponsiveContainer, Tooltip, XAxis, YAxis } from "recharts";
import { clock } from "@/lib/siaga";

const Tip = ({ active, payload, label }) => {
  if (!active || !payload?.length) return null;
  return (
    <div className="bg-white border border-slate-200 rounded-lg shadow-lg px-3 py-2 text-xs">
      <p className="text-slate-500 mb-1 mono">{clock(label)}</p>
      {payload.map((p) => (
        <p key={p.dataKey} className="font-semibold" style={{ color: p.color }}>{p.name}: {typeof p.value === "number" ? +p.value.toFixed(3) : p.value}</p>
      ))}
    </div>
  );
};

const axis = { stroke: "#94A3B8", tick: { fontSize: 11, fill: "#64748B" }, tickLine: false, axisLine: false };

export const TrendChart = ({ data, lines, height = 260, testId }) => {
  const dual = lines.some((l) => l.right);
  return (
    <div data-testid={testId} style={{ height }} className="w-full">
      <ResponsiveContainer width="100%" height="100%">
        <LineChart data={data} margin={{ top: 8, right: dual ? 0 : 8, left: -12, bottom: 0 }}>
          <CartesianGrid stroke="#EEF1EC" vertical={false} />
          <XAxis dataKey="ts" tickFormatter={clock} minTickGap={48} {...axis} />
          <YAxis yAxisId="l" domain={["auto", "auto"]} width={48} {...axis} />
          {dual && <YAxis yAxisId="r" orientation="right" domain={["auto", "auto"]} width={48} {...axis} />}
          <Tooltip content={<Tip />} cursor={{ stroke: "#CBD5E1", strokeDasharray: "4 4" }} />
          {lines.map((l) => (
            <Line key={l.key} yAxisId={l.right ? "r" : "l"} type="monotone" dataKey={l.key} name={l.name} stroke={l.color}
              strokeWidth={2.5} dot={false} activeDot={{ r: 4 }} isAnimationActive animationDuration={350} connectNulls />
          ))}
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
};
