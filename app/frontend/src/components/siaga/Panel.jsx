export const Panel = ({ title, subtitle, icon: Icon, children, className = "", testId, right, id }) => (
  <section id={id} data-testid={testId} className={`card-pro rise p-5 md:p-6 scroll-mt-24 ${className}`}>
    <header className="flex items-start justify-between gap-3 mb-5">
      <div className="flex items-center gap-3 min-w-0">
        {Icon && (
          <span className="w-9 h-9 rounded-xl bg-emerald-50 text-emerald-700 grid place-items-center shrink-0">
            <Icon className="w-[18px] h-[18px]" strokeWidth={1.9} />
          </span>
        )}
        <div className="min-w-0">
          <h3 className="text-[15px] font-bold text-slate-900 leading-tight truncate">{title}</h3>
          {subtitle && <p className="text-xs text-slate-500 mt-0.5 truncate">{subtitle}</p>}
        </div>
      </div>
      {right && <div className="flex items-center gap-2 shrink-0">{right}</div>}
    </header>
    {children}
  </section>
);

export const Chip = ({ children, color, testId, solid }) => (
  <span data-testid={testId}
    className="inline-flex items-center gap-1.5 text-[11px] font-bold tracking-wide px-2.5 py-1 rounded-full"
    style={solid ? { background: color, color: "#fff" } : { background: `${color}14`, color, boxShadow: `inset 0 0 0 1px ${color}33` }}>
    {children}
  </span>
);
