# examples/

可运行的 Verilog 示例（仓库根目录）。演示用；回归用黄金用例在 `tests/parse/`。

| 文件 | 说明 |
|------|------|
| `inv.v` | 组合反相器 |
| `counter.v` | 4-bit 同步计数器 |
| `alu4.v` | 4-bit ALU（多输入/输出） |
| `combo_bus.v` | 组合多进多出 |
| `sum_1_to_100.v` | 累加 1…100 → `sum=5050` |
| `matmul2x2.v` | 2×2 矩阵乘（扁平端口）→ `[[19,22],[43,50]]`（含 `// @vs` 矩阵视图注解） |
| `matmul.v` | 参数化矩阵乘（`parameter` + `for` + 2D unpacked；元素名 `a_i_j`） |

```bash
./build/vs run examples/counter.v --threads 4 --until 200 \
  --clock clk=10 --reset rst=20 --watch q --trace build/trace.jsonl
```

矩阵示例可视化：

```bash
./build/vs run examples/matmul2x2.v --threads 2 --until 40 \
  --force a00=1 --force a01=2 --force a10=3 --force a11=4 \
  --force b00=5 --force b01=6 --force b10=7 --force b11=8 \
  --watch c00,c01,c10,c11 --trace build/trace_matmul.jsonl
cd tools/vs-view && npm run build && node dist/server.js --trace ../../build/trace_matmul.jsonl
```

参数化 matmul（元素公开名 `base_i_j`）：

```bash
./build/vs run examples/matmul.v --threads 2 --until 40 \
  --force a_0_0=1 --force a_0_1=2 --force a_1_0=3 --force a_1_1=4 \
  --force b_0_0=5 --force b_0_1=6 --force b_1_0=7 --force b_1_1=8 \
  --watch c_0_0,c_0_1,c_1_0,c_1_1
```

`// @vs` 注解（`view` / `op` / `expr`）会写入 trace `meta`，由 vs-view 在原始端口板下方渲染矩阵。详见 `docs/superpowers/specs/2026-09-17-typed-views-anno-design.md`。

新增示例时：放在本目录、更新本 README，并视需要加 `tests/sim` / CTest。
