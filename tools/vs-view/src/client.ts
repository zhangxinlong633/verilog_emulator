type TraceEvent = {
  t: number;
  d: number;
  tid: number;
  op: string;
  sig?: string;
  val?: string;
  proc?: string;
  msg?: string;
  module?: string;
  ports?: PortInfo[];
  procs?: ProcInfo[];
};

type PortInfo = {
  name: string;
  dir: string;
  width: number;
  reg?: boolean;
};

type ProcInfo = {
  name: string;
  kind: string;
};

type TracePayload = {
  events: TraceEvent[];
  signals: string[];
  waves: Record<string, { t: number; v: string }[]>;
  module?: string;
  ports?: PortInfo[];
  procs?: ProcInfo[];
  maxT: number;
};

function valueAt(waves: Record<string, { t: number; v: string }[]>, sig: string, t: number): string {
  const pts = waves[sig] || [];
  let v = "x";
  for (const p of pts) {
    if (p.t <= t) v = p.v;
    else break;
  }
  return v;
}

function renderModule(data: TracePayload, t: number): void {
  const board = document.getElementById("module-board");
  if (!board) return;

  const ports = data.ports || [];
  const inputs = ports.filter((p) => p.dir === "input" || p.dir === "inout");
  const outputs = ports.filter((p) => p.dir === "output" || p.dir === "inout");
  const internals = ports.filter((p) => p.dir === "internal");

  const pinHtml = (p: PortInfo): string => {
    const val = valueAt(data.waves, p.name, t);
    const w = p.width > 1 ? `[${p.width - 1}:0]` : "";
    const reg = p.reg ? " reg" : "";
    return `<div class="pin ${p.dir}" data-sig="${p.name}">
      <div><span class="name">${p.name}</span><span class="meta">${p.dir}${w}${reg}</span></div>
      <div class="val">${val}</div>
    </div>`;
  };

  const procs = (data.procs || [])
    .map((p) => `<span title="${p.kind}">${p.kind}: ${p.name}</span>`)
    .join("");

  board.innerHTML = `
    <div class="pin-col">
      <h3>Inputs</h3>
      ${inputs.map(pinHtml).join("") || "<p class='meta'>（无）</p>"}
      ${internals.length ? `<h3 style="margin-top:1rem">Internal</h3>${internals.map(pinHtml).join("")}` : ""}
    </div>
    <div class="module-box">
      <div class="mod-name">${data.module || "module"}</div>
      <div class="procs">${procs || "<span>no processes</span>"}</div>
    </div>
    <div class="pin-col">
      <h3>Outputs</h3>
      ${outputs.map(pinHtml).join("") || "<p class='meta'>（无）</p>"}
    </div>
  `;
}

function renderTimeline(data: TracePayload, t: number): void {
  const timeline = document.getElementById("timeline");
  if (!timeline) return;
  timeline.innerHTML = "";
  const colors = ["#0b6e4f", "#b35c00", "#2a5ea8", "#8b2c5c", "#444"];
  let focus: HTMLElement | null = null;
  for (const e of data.events) {
    if (e.op === "meta") continue;
    const div = document.createElement("div");
    div.className = "ev" + (e.t === t ? " active" : "");
    const tidColor = colors[e.tid % colors.length];
    const bits = [
      `t=${e.t}`,
      `d=${e.d}`,
      `<span class="tid" style="color:${tidColor}">tid=${e.tid}</span>`,
      e.op,
    ];
    if (e.proc) bits.push(e.proc);
    if (e.sig) bits.push(`${e.sig}=${e.val ?? ""}`);
    if (e.msg) bits.push(e.msg);
    div.innerHTML = bits.join(" · ");
    timeline.appendChild(div);
    if (e.t === t) focus = div;
  }
  if (focus) focus.scrollIntoView({ block: "nearest" });
}

function renderWaves(data: TracePayload, tCursor: number): void {
  const canvas = document.getElementById("waves") as HTMLCanvasElement | null;
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  if (!ctx) return;

  const W = canvas.width;
  const H = canvas.height;
  const colors = ["#0b6e4f", "#b35c00", "#2a5ea8", "#8b2c5c", "#444"];
  ctx.clearRect(0, 0, W, H);

  const signals = data.signals || [];
  const rowH = Math.floor(H / Math.max(signals.length, 1));
  const maxT = Math.max(data.maxT, 1);
  const plotW = W - 100;

  signals.forEach((sig, i) => {
    const y0 = i * rowH + 8;
    const y1 = y0 + rowH - 16;
    const mid = (y0 + y1) / 2;
    ctx.fillStyle = "#5c564c";
    ctx.font = "12px IBM Plex Mono, monospace";
    ctx.fillText(sig, 8, mid + 4);
    const pts = data.waves[sig] || [];
    if (!pts.length) return;
    ctx.strokeStyle = colors[i % colors.length];
    ctx.lineWidth = 2;
    ctx.beginPath();
    let lastV = pts[0].v;
    for (let k = 0; k < pts.length; k++) {
      const x = 80 + (pts[k].t / maxT) * plotW;
      const y = lastV !== "0" ? y0 + 4 : y1 - 4;
      if (k === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
      const y2 = pts[k].v !== "0" ? y0 + 4 : y1 - 4;
      ctx.lineTo(x, y2);
      lastV = pts[k].v;
    }
    ctx.lineTo(80 + plotW, lastV !== "0" ? y0 + 4 : y1 - 4);
    ctx.stroke();
  });

  const cx = 80 + (tCursor / maxT) * plotW;
  ctx.strokeStyle = "rgba(28,26,22,0.45)";
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 4]);
  ctx.beginPath();
  ctx.moveTo(cx, 0);
  ctx.lineTo(cx, H);
  ctx.stroke();
  ctx.setLineDash([]);
}

async function main(): Promise<void> {
  const res = await fetch("/api/trace");
  const data = (await res.json()) as TracePayload;

  const timeInput = document.getElementById("time") as HTMLInputElement | null;
  const timeLabel = document.getElementById("time-label");
  if (!timeInput || !timeLabel) return;

  timeInput.max = String(data.maxT);
  timeInput.value = "0";

  let playing = false;
  let timer: number | undefined;

  const paint = (): void => {
    const t = Number(timeInput.value);
    timeLabel.textContent = String(t);
    renderModule(data, t);
    renderTimeline(data, t);
    renderWaves(data, t);
  };

  timeInput.addEventListener("input", paint);
  document.getElementById("btn-prev")?.addEventListener("click", () => {
    timeInput.value = String(Math.max(0, Number(timeInput.value) - 1));
    paint();
  });
  document.getElementById("btn-next")?.addEventListener("click", () => {
    timeInput.value = String(Math.min(data.maxT, Number(timeInput.value) + 1));
    paint();
  });
  document.getElementById("btn-play")?.addEventListener("click", () => {
    playing = !playing;
    const btn = document.getElementById("btn-play");
    if (btn) btn.textContent = playing ? "❚❚" : "▶";
    if (playing) {
      timer = window.setInterval(() => {
        const next = Number(timeInput.value) + 1;
        if (next > data.maxT) {
          playing = false;
          if (btn) btn.textContent = "▶";
          if (timer) window.clearInterval(timer);
          return;
        }
        timeInput.value = String(next);
        paint();
      }, 80);
    } else if (timer) {
      window.clearInterval(timer);
    }
  });

  paint();
}

main().catch((err: unknown) => {
  const el = document.getElementById("module-board");
  if (el) el.textContent = String(err);
});
