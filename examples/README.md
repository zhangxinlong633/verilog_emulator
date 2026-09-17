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
| `matmul.v` | 参数化矩阵乘（`parameter` + `for` + 2D unpacked；元素名 `a_i_j`；`@vs array=`） |
| `npu_transformer.v` | 玩具 NPU：时序 Transformer 层（硬注意力 + FFN/ReLU），实例化 `blas/`；默认 `Y=[[3,4],[3,4]]` |
| `blas/` | 组合 BLAS 叶子：`blas_gemm` / `blas_gemm_bt` / `blas_relu` / `blas_row_argmax` |

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

玩具 NPU Transformer（需带上 `blas/` 叶子；复位后自动开跑；建议 `--threads 1`）：

```bash
./build/vs run examples/npu_transformer.v \
  examples/blas/gemm.v examples/blas/gemm_bt.v \
  examples/blas/relu.v examples/blas/row_argmax.v \
  --threads 1 --until 400 \
  --clock clk=10 --reset rst=20 \
  --force x_0_0=1 --force x_0_1=2 --force x_1_0=3 --force x_1_1=4 \
  --force wq_0_0=1 --force wq_0_1=0 --force wq_1_0=0 --force wq_1_1=1 \
  --force wk_0_0=1 --force wk_0_1=0 --force wk_1_0=0 --force wk_1_1=1 \
  --force wv_0_0=1 --force wv_0_1=0 --force wv_1_0=0 --force wv_1_1=1 \
  --force w1_0_0=1 --force w1_0_1=0 --force w1_1_0=0 --force w1_1_1=1 \
  --force w2_0_0=1 --force w2_0_1=0 --force w2_1_0=0 --force w2_1_1=1 \
  --watch done,phase,y_0_0,y_0_1,y_1_0,y_1_1 \
  --trace build/trace_npu.jsonl
# 期望: done=1  Y=[[3,4],[3,4]]
cd tools/vs-view && npm run build && node dist/server.js --trace ../../build/trace_npu.jsonl
```

`// @vs` 注解（`view` / `op` / `expr`）会写入 trace `meta`，由 vs-view 在原始端口板下方渲染矩阵。`view matrix` 可用显式 `cells=`，或 `array=base rows=N cols=K`（参数名在 elab 后展开为 `base_i_j`）。详见 `docs/superpowers/specs/2026-09-17-typed-views-anno-design.md` 与 `docs/superpowers/specs/2026-09-17-param-matmul-arrays-design.md`；NPU 设计见 `docs/superpowers/specs/2026-09-17-npu-transformer-design.md`；BLAS 层次见 `docs/superpowers/specs/2026-09-17-npu-blas-hierarchy-design.md`。

新增示例时：放在本目录、更新本 README，并视需要加 `tests/sim` / CTest。
