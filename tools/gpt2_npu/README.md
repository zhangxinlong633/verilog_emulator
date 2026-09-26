# tools/gpt2_npu

Export a real GPT-2 `mlp.c_fc` activation/weight tile for `examples/gpt2_npu/`.

```bash
pip install -r tools/gpt2_npu/requirements.txt
# If default HF mirror fails: HF_ENDPOINT=https://huggingface.co
HF_HUB_OFFLINE=0 python3 tools/gpt2_npu/export_tile.py --out-dir examples/gpt2_npu
# optional: --n 4 --k 32 --m 32 --prompt Hello --layer 0 --model openai-community/gpt2
```

CTest uses committed generates (no HuggingFace download in CI).
