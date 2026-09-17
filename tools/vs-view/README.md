# tools/vs-view/

TypeScript 可视化：读取 `vs run --trace` 产生的 JSONL，提供时间线与精简波形（模块 I/O 板）。

```bash
cd tools/vs-view && npm install && npm run build
node dist/server.js --trace ../../build/trace.jsonl --port 8787
```

源码在 `src/`，静态页在 `public/`。
