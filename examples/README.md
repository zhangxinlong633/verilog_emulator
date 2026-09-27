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
| `gen_mac2.v` | `generate for` 展开的 2 点积（局部线 `g_0_p`）+ `generate if`；`y=39` |
| `gen_buf2.v` | `generate for` 实例数组（每次迭代用局部线接到 `buf8`） |
| `npu_transformer.v` | 玩具 NPU：时序 Transformer 层（硬注意力 + FFN/ReLU），实例化 `blas/`；默认 `Y=[[3,4],[3,4]]` |
| `tiny_npu/` | 更丰富 Tiny NPU（T=D=4, HF=8）：端到端 + GEMM/MLP/Attn 拆分 demo；权重由 `tools/tiny_npu/gen_model.py` 生成 |
| `gpt2_npu/` | Host 跑完整 GPT-2，RTL 跑 layer0 `mlp.c_fc` 真实 int8 tile（`tools/gpt2_npu/export_tile.py`） |
| `fpga_gemm3/` | **ASIC 风格**矩阵乘：叶 MAC≤32 可仿真；分块至 1024；**T=64 产品定档**（纸面外推，含 die/成本量级） |
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

Tiny NPU（T=D=4, HF=8；权重已生成，建议 `--threads 1`）：

```bash
./build/vs run examples/tiny_npu/tiny_transformer.v examples/tiny_npu/weights.v \
  examples/blas/gemm.v examples/blas/gemm_bt.v \
  examples/blas/relu.v examples/blas/row_argmax.v \
  --threads 1 --until 400 --clock clk=10 --reset rst=20 \
  $(cat examples/tiny_npu/forces.txt | sed 's/^/--force /') \
  --watch done,y_0_0,y_0_1,y_0_2,y_0_3
# 期望: done=1  Y[0]=[17594,19180,17936,19999]
```

GPT-2 × NPU tile（Host 完整 GPT-2；RTL 只跑真实 `c_fc` tile）：

```bash
./build/vs run examples/gpt2_npu/gemm_tile.v examples/blas/gemm.v \
  --threads 1 --until 40 --watch "$(cat examples/gpt2_npu/watch.txt)"
# CTest: sim.gpt2_npu_gemm
```

`// @vs` 注解（`view` / `op` / `expr`）会写入 trace `meta`，由 vs-view 在原始端口板下方渲染矩阵。`view matrix` 可用显式 `cells=`，或 `array=base rows=N cols=K`（参数名在 elab 后展开为 `base_i_j`）。详见 `docs/superpowers/specs/2026-09-17-typed-views-anno-design.md` 与 `docs/superpowers/specs/2026-09-17-param-matmul-arrays-design.md`；NPU 设计见 `docs/superpowers/specs/2026-09-17-npu-transformer-design.md`；BLAS 层次见 `docs/superpowers/specs/2026-09-17-npu-blas-hierarchy-design.md`。

新增示例时：放在本目录、更新本 README，并视需要加 `tests/sim` / CTest。
