# vs — Verilog Simulator

从零实现的 **Verilog 模拟器**（解释执行、事件驱动），程序名为 **`vs`**。

代理约束：[`AGENTS.md`](AGENTS.md)、[`CLAUDE.md`](CLAUDE.md)。示例在 [`examples/`](examples/)。

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
./build/vs run examples/counter.v \
  --threads 4 --until 200 --clock clk=10 --reset rst=20 \
  --watch q,clk,rst --trace build/trace.jsonl
```

## 多端口示例

```bash
# 4-bit ALU：输入 a/b/op，输出 y/zflag
./build/vs run examples/alu4.v --threads 4 --until 120 \
  --clock clk=10 --reset rst=20 \
  --force a=3 --force b=5 --force op=0 \
  --watch y,zflag,a,b,op --trace build/trace_alu4.jsonl

# 组合逻辑多进多出
./build/vs run examples/combo_bus.v --threads 2 --until 40 \
  --force a=5 --force b=3 --force c=1 --force sel=1 \
  --watch a,b,c,sel,sum,mix,aand,eq_ab,gt --trace build/trace_combo.jsonl

# TypeScript 可视化（左侧输入 / 中间模块 / 右侧输出）
cd tools/vs-view && npm run build
node dist/server.js --trace ../../build/trace_alu4.jsonl --port 8787
```

公式核对：ALU 里 `a=3,b=5,op=0` → `y = 3+5 = 8` 正确。

累加 1…100（`sum = 5050`）：

```bash
./build/vs run examples/sum_1_to_100.v --threads 4 --until 1200 \
  --clock clk=10 --reset rst=20 --watch sum,i,done \
  --trace build/trace_sum100.jsonl
# 期望: sum=5050 i=101 done=1
```

2×2 矩阵乘（`C = [[19,22],[43,50]]`）：

```bash
./build/vs run examples/matmul2x2.v --threads 2 --until 40 \
  --force a00=1 --force a01=2 --force a10=3 --force a11=4 \
  --force b00=5 --force b01=6 --force b10=7 --force b11=8 \
  --watch c00,c01,c10,c11 --trace build/trace_matmul.jsonl
```

## 目录

| 路径 | 说明 |
|------|------|
| [`examples/`](examples/) | 可运行 Verilog 示例 |
| [`src/`](src/) | C11 实现 |
| [`include/`](include/) | 公共头文件 |
| [`tests/`](tests/) | 回归测试 |
| [`tools/`](tools/) | `vs-view` 等工具 |
| [`docs/`](docs/) | 设计与说明文档 |

各目录均有 `README.md`。

## 文档

- [`docs/README.md`](docs/README.md)
- Design：[`docs/superpowers/specs/2026-09-17-verilog-emulator-design.md`](docs/superpowers/specs/2026-09-17-verilog-emulator-design.md)
- MT sim：[`docs/superpowers/specs/2026-09-17-mt-sim-trace-view-design.md`](docs/superpowers/specs/2026-09-17-mt-sim-trace-view-design.md)
