import { MapContainer, TileLayer, CircleMarker, Popup } from "react-leaflet";
import "leaflet/dist/leaflet.css";
import { Map as MapIcon } from "lucide-react";
import { Panel, Chip } from "./Panel";
import { NODE_LOCATION, riskMeta, fmt } from "@/lib/siaga";

export const NodeMap = ({ latest, online }) => {
  const m = riskMeta(latest?.risk);
  const pos = [NODE_LOCATION.lat, NODE_LOCATION.lng];
  return (
    <Panel id="peta" title="Peta Pemantauan" subtitle={`${NODE_LOCATION.name} · ${NODE_LOCATION.lat}, ${NODE_LOCATION.lng}`} icon={MapIcon}
      testId="map-panel" right={<Chip color={m.color} testId="map-risk-chip">{m.label}</Chip>}>
      <div className="h-[400px] rounded-xl overflow-hidden border border-slate-200 relative z-0" data-testid="node-map">
        <MapContainer center={pos} zoom={13} scrollWheelZoom={false} className="h-full w-full">
          <TileLayer url="https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png" maxZoom={17}
            attribution='Map data: &copy; OpenStreetMap contributors, OpenTopoMap' />
          {latest?.risk === 2 && <CircleMarker center={pos} radius={22} pathOptions={{ color: m.color, fill: false, className: "pulse-ring" }} />}
          <CircleMarker center={pos} radius={11} pathOptions={{ color: "#fff", weight: 3, fillColor: online ? m.color : "#94A3B8", fillOpacity: 1 }}>
            <Popup>
              <div className="text-xs space-y-0.5">
                <p className="font-bold text-slate-900">{latest?.device || "SIAGA-HUTAN"}</p>
                <p>Status: <b style={{ color: m.color }}>{m.label}</b></p>
                <p>Suhu {fmt(latest?.temp, 1)}°C · RH {fmt(latest?.hum, 1)}%</p>
                <p>Servo {latest?.servo ?? "—"}°</p>
              </div>
            </Popup>
          </CircleMarker>
        </MapContainer>
      </div>
    </Panel>
  );
};
