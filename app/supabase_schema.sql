-- Jalankan di Supabase Dashboard -> SQL Editor -> New query -> Run
create table if not exists public.telemetry (
  id bigserial primary key,
  created_at timestamptz not null default now(),
  device text,
  seq integer,
  temp double precision,
  hum double precision,
  dust double precision,
  gas_v double precision,
  gas_rs double precision,
  rms double precision,
  risk smallint,
  servo integer,
  fan_speed integer,
  cam_ack boolean,
  raw jsonb
);
create index if not exists telemetry_created_at_idx on public.telemetry (created_at desc);
create index if not exists telemetry_risk_idx on public.telemetry (risk);

create table if not exists public.alerts (
  id bigserial primary key,
  created_at timestamptz not null default now(),
  device text,
  kind text not null default 'alert',
  level smallint,
  message text,
  raw jsonb
);
create index if not exists alerts_created_at_idx on public.alerts (created_at desc);

alter table public.telemetry enable row level security;
alter table public.alerts enable row level security;

drop policy if exists "anon insert telemetry" on public.telemetry;
drop policy if exists "anon select telemetry" on public.telemetry;
drop policy if exists "anon insert alerts" on public.alerts;
drop policy if exists "anon select alerts" on public.alerts;

create policy "anon insert telemetry" on public.telemetry for insert to anon with check (true);
create policy "anon select telemetry" on public.telemetry for select to anon using (true);
create policy "anon insert alerts" on public.alerts for insert to anon with check (true);
create policy "anon select alerts" on public.alerts for select to anon using (true);

notify pgrst, 'reload schema';
