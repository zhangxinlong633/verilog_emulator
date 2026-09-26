# GPT-2 NPU GEMM tile

Host runs **full GPT-2** (`openai-community/gpt2`); this directory is one **real** `layer0.mlp.c_fc` tile
(quantized int8 → int32 GEMM) for `vs` + `blas_gemm`.

| File | Role |
|------|------|
| `gemm_tile.v` | Combinational top (generated) |
| `expect_c.inc` / `watch.txt` | CTest |
| `golden.json` | Scales, offsets, full A/B/C |

Regenerate:

```bash
pip install -r tools/gpt2_npu/requirements.txt
python3 tools/gpt2_npu/export_tile.py --out-dir examples/gpt2_npu
```

Run:

```bash
./build/vs run examples/gpt2_npu/gemm_tile.v examples/blas/gemm.v \
  --threads 1 --until 40 --watch "$(cat examples/gpt2_npu/watch.txt)"
```

Default tile: N=4 K=32 M=32 (kept modest for sim pending limits).
