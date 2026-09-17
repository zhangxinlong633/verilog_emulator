# tools/vs-view/

TypeScript 可视化：读取 `vs run --trace` 的 JSONL，提供模块 I/O 板、时间线、精简波形。

若 meta 含 `views` / `ops` / `exprs`（来自 Verilog `// @vs` 注解），在原始端口板下方渲染 **Typed views**（如矩阵）；点击格子可展开代入当前值的表达式。

```bash
cd tools/vs-view && npm install && npm run build
node dist/server.js --trace ../../build/trace_matmul.jsonl --port 8787
```

源码在 `src/`，静态页在 `public/`。
