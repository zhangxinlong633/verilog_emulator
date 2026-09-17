# CLAUDE.md

本仓库给 Claude / Cursor Agent 的约束。与 [`AGENTS.md`](AGENTS.md) **内容对齐**；改一处时请同步另一处。

## 必须遵守

1. **示例位置**：Verilog 可运行示例只放在仓库根目录 `examples/`，不要放在 `docs/examples/`。
2. **目录 README**：每个主要目录（含新建目录）都要有 `README.md`，说明用途与入口。
3. **路径引用**：文档、CMake、测试脚本中的示例路径使用 `examples/...`。
4. **文档优先**：行为细节写在 `docs/`；`include/vs/*.h` 只保留简短 API 注释。
5. **范围克制**：只改任务相关文件；除本约定要求的 README / 代理约束文件外，不主动堆 markdown。

## 常用入口

```bash
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure
./build/vs run examples/counter.v --threads 4 --until 200 \
  --clock clk=10 --reset rst=20 --watch q --trace build/trace.jsonl
```

更多：根 [`README.md`](README.md)、[`docs/README.md`](docs/README.md)、[`AGENTS.md`](AGENTS.md)。
