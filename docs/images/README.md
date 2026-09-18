# docs/images/

vs-view 演示截图，供根 [`README.md`](../../README.md) 与文档引用。

| 文件 | 内容 |
|------|------|
| `blas-gemm-vs-view.png` | 硬件风格 `blas_gemm`：`C=A·B` → `[[19,22],[43,50]]` |
| `matmul-vs-view.png` | 参数化 `matmul` + Typed Views 矩阵板 |
| `npu-transformer-vs-view.png` | 玩具 NPU（实例化 BLAS 叶子）+ Typed Views（X/Q/K/V/S/A/O/Y） |
| `tiny-npu-vs-view.png` | Tiny NPU（T=D=4, HF=8）端到端 + Typed Views（含硬注意力 A 与 Y） |

重新截取（需本机 Chrome + 已构建的 `tools/vs-view`）：

```bash
./build/vs run examples/blas/gemm.v ... --trace build/trace_blas_gemm.jsonl
./build/vs run examples/matmul.v ... --trace build/trace_matmul_param.jsonl
./build/vs run examples/npu_transformer.v examples/blas/*.v ... --trace build/trace_npu.jsonl
./build/vs run examples/tiny_npu/tiny_transformer.v examples/tiny_npu/weights.v \
  examples/blas/*.v ... --trace build/trace_tiny.jsonl
# 再用 headless Chrome / puppeteer 打开 vs-view 截屏
```
