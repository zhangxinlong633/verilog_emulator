type TraceEvent = {
  t: number;
  d: number;
  tid: number;
  op: string;
  sig?: string;
  val?: string;
  proc?: string;
  msg?: string;
};

type TracePayload = {
  events: TraceEvent[];
  signals: string[];
  waves: Record<string, { t: number; v: string }[]>;
};

async function main(): Promise<void> {
  const res = await fetch("/api/trace");
  const data = (await res.json()) as TracePayload;
  const timeline = document.getElementById("timeline");
  if (!timeline) return;

  const colors = ["#0b6e4f", "#b35c00", "#2a5ea8", "#8b2c5c", "#444"];

  for (const e of data.events) {
    const div = document.createElement("div");
    div.className = "ev";
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
  }

  const canvas = document.getElementById("waves") as HTMLCanvasElement | null;
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  if (!ctx) return;

  const W = canvas.width;
  const H = canvas.height;
  ctx.clearRect(0, 0, W, H);
  ctx.fillStyle = "#1c1a16";
  ctx.font = "12px IBM Plex Mono, monospace";

  const signals = data.signals || [];
  const rowH = Math.floor(H / Math.max(signals.length, 1));
  let maxT = 1;
  for (const s of signals) {
    for (const p of data.waves[s] || []) {
      if (p.t > maxT) maxT = p.t;
    }
  }

  signals.forEach((sig, i) => {
    const y0 = i * rowH + 8;
    const y1 = y0 + rowH - 16;
    const mid = (y0 + y1) / 2;
    ctx.fillStyle = "#5c564c";
    ctx.fillText(sig, 8, mid + 4);
    const pts = data.waves[sig] || [];
    if (!pts.length) return;
    ctx.strokeStyle = colors[i % colors.length];
    ctx.lineWidth = 2;
    ctx.beginPath();
    let lastV = pts[0].v;
    const plotW = W - 100;
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
}

main().catch((err: unknown) => {
  const el = document.getElementById("timeline");
  if (el) el.textContent = String(err);
});
