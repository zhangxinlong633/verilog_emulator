# tools/

与 `vs` 配套的辅助工具（非仿真核本身）。

| 路径 | 说明 |
|------|------|
| `vs-view/` | TypeScript JSONL trace 时间线 / 波形可视化 |
| `tiny_npu/` | 生成 `examples/tiny_npu/` 权重、golden、拆分 demo |
| `gpt2_npu/` | 从 HuggingFace GPT-2 导出真实 `c_fc` tile 到 `examples/gpt2_npu/` |
