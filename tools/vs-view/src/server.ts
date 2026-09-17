import * as fs from "node:fs";
import * as http from "node:http";
import * as path from "node:path";
import { fileURLToPath } from "node:url";

export type TraceEvent = {
  t: number;
  d: number;
  tid: number;
  op: string;
  sig?: string;
  val?: string;
  proc?: string;
  msg?: string;
};

function parseArgs(argv: string[]): { trace: string; port: number } {
  let trace = "";
  let port = 8787;
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === "--trace" && i + 1 < argv.length) {
      trace = argv[++i];
    } else if (argv[i] === "--port" && i + 1 < argv.length) {
      port = Number(argv[++i]);
    } else if (argv[i] === "--help" || argv[i] === "-h") {
      console.log("Usage: node dist/server.js --trace <file.jsonl> [--port 8787]");
      process.exit(0);
    }
  }
  if (!trace) {
    console.error("vs-view: --trace is required");
    process.exit(2);
  }
  return { trace, port };
}

export function loadTrace(filePath: string): TraceEvent[] {
  const text = fs.readFileSync(filePath, "utf8");
  const events: TraceEvent[] = [];
  for (const line of text.split(/\r?\n/)) {
    const s = line.trim();
    if (!s) continue;
    try {
      events.push(JSON.parse(s) as TraceEvent);
    } catch {
      /* skip bad lines */
    }
  }
  return events;
}

function waveFromEvents(events: TraceEvent[], signals: string[]): Record<string, { t: number; v: string }[]> {
  const waves: Record<string, { t: number; v: string }[]> = {};
  for (const s of signals) {
    waves[s] = [];
  }
  for (const e of events) {
    if ((e.op === "commit" || e.op === "clock" || e.op === "reset" || e.op === "ba") && e.sig && e.val !== undefined) {
      if (waves[e.sig]) {
        waves[e.sig].push({ t: e.t, v: e.val });
      }
    }
  }
  return waves;
}

const { trace, port } = parseArgs(process.argv.slice(2));
const __dirname = path.dirname(fileURLToPath(import.meta.url));
const publicDir = path.join(__dirname, "..", "public");

const server = http.createServer((req, res) => {
  const url = new URL(req.url ?? "/", `http://127.0.0.1:${port}`);

  if (url.pathname === "/api/trace") {
    try {
      const events = loadTrace(trace);
      const sigSet = new Set<string>();
      for (const e of events) {
        if (e.sig) sigSet.add(e.sig);
      }
      const signals = [...sigSet].slice(0, 4);
      const body = JSON.stringify({
        events,
        signals,
        waves: waveFromEvents(events, signals),
      });
      res.writeHead(200, {
        "Content-Type": "application/json; charset=utf-8",
        "Cache-Control": "no-store",
      });
      res.end(body);
    } catch (err) {
      res.writeHead(500, { "Content-Type": "text/plain" });
      res.end(String(err));
    }
    return;
  }

  let filePath = path.join(publicDir, url.pathname === "/" ? "index.html" : url.pathname);
  if (!filePath.startsWith(publicDir)) {
    res.writeHead(403);
    res.end("forbidden");
    return;
  }
  fs.readFile(filePath, (err, data) => {
    if (err) {
      res.writeHead(404);
      res.end("not found");
      return;
    }
    const ext = path.extname(filePath);
    const type =
      ext === ".html"
        ? "text/html; charset=utf-8"
        : ext === ".css"
          ? "text/css; charset=utf-8"
          : ext === ".js"
            ? "text/javascript; charset=utf-8"
            : "application/octet-stream";
    res.writeHead(200, { "Content-Type": type });
    res.end(data);
  });
});

server.listen(port, () => {
  console.log(`vs-view listening on http://127.0.0.1:${port}`);
  console.log(`trace: ${path.resolve(trace)}`);
});
