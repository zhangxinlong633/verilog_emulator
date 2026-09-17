# vs — Verilog Simulator

从零实现的 **Verilog 模拟器**（解释执行、事件驱动），程序名为 **`vs`**。

## 当前能力

- **P0**：flex + bison 解析与 AST dump
- **P2-lite-MT**：最小多线程仿真、`JSONL` trace、TypeScript 可视化

## 依赖

- CMake ≥ 3.16、C11、flex、**bison ≥ 3.0**（macOS：`brew install bison`）
- 可视化：Node.js ≥ 18、npm（TypeScript）

```bash
brew install bison   # macOS
```

## 快速开始

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

# 仿真计数器（多线程 + trace）
./build/vs run docs/examples/counter.v \
  --threads 4 --until 200 --clock clk=10 --reset rst=20 \
  --watch q,clk,rst --trace build/trace.jsonl

## 多端口示例

```bash
# 4-bit ALU：输入 a/b/op，输出 y/zflag
./build/vs run docs/examples/alu4.v --threads 4 --until 120 \
  --clock clk=10 --reset rst=20 \
  --force a=3 --force b=5 --force op=0 \
  --watch y,zflag,a,b,op --trace build/trace_alu4.jsonl

# 组合逻辑多进多出
./build/vs run docs/examples/combo_bus.v --threads 2 --until 40 \
  --force a=5 --force b=3 --force c=1 --force sel=1 \
  --watch a,b,c,sel,sum,mix,aand,eq_ab,gt --trace build/trace_combo.jsonl

cd tools/vs-view && npm run build
node dist/server.js --trace ../../build/trace_alu4.jsonl --port 8787
```
```

## 文档

- [`docs/README.md`](docs/README.md)
- Design：[`docs/superpowers/specs/2026-09-17-verilog-emulator-design.md`](docs/superpowers/specs/2026-09-17-verilog-emulator-design.md)
- MT sim：[`docs/superpowers/specs/2026-09-17-mt-sim-trace-view-design.md`](docs/superpowers/specs/2026-09-17-mt-sim-trace-view-design.md)
