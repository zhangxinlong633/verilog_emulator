# Tiny NPU workloads

Deterministic **toy-quantized** Transformer block (hard attention + FFN), plus split workloads.
Weights/golden from `tools/tiny_npu/gen_model.py` (seed `20260918`).

| File | Role |
|------|------|
| `tiny_transformer.v` | End-to-end FSM top (T=4, D=4, HF=8) |
| `weights.v` | Generated weight pack (`tiny_npu_weights`) |
| `demo_gemm.v` | GEMM-only: `X · Wq` |
| `demo_mlp.v` | FFN: `O·W1 → ReLU → ·W2` |
| `demo_attn.v` | Hard attention: `QKᵀ → argmax → A·V` |
| `forces.txt` / `expect_*.inc` | CLI forces + CTest expects |
| `golden.json` | Full reference tensors |

vs-view 截图见 [`docs/images/tiny-npu-vs-view.png`](../../docs/images/tiny-npu-vs-view.png)。

```bash
# regenerate weights, demos, golden
python3 tools/tiny_npu/gen_model.py

# full tiny transformer (use --threads 1)
./build/vs run examples/tiny_npu/tiny_transformer.v examples/tiny_npu/weights.v \
  examples/blas/gemm.v examples/blas/gemm_bt.v \
  examples/blas/relu.v examples/blas/row_argmax.v \
  --threads 1 --until 400 --clock clk=10 --reset rst=20 \
  $(cat examples/tiny_npu/forces.txt | sed 's/^/--force /') \
  --watch done,y_0_0,y_0_1,y_0_2,y_0_3

# split demos
./build/vs run examples/tiny_npu/demo_gemm.v examples/blas/gemm.v \
  --threads 1 --until 40 --watch c_0_0,c_0_1,c_0_2,c_0_3
```
