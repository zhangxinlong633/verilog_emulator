# tools/tiny_npu

Generate deterministic toy weights + golden tensors + split demo RTL for `examples/tiny_npu/`.

```bash
python3 tools/tiny_npu/gen_model.py
# optional: --seed N --out-dir path
```

Emits `weights.v`, `demo_{gemm,attn,mlp}.v`, `forces.txt`, `expect_*.inc`, `golden.json`.
