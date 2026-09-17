# BLAS-style RTL leaves for `vs`

Combinational operators used by `examples/npu_transformer.v`.
Instantiate with named ports; connect whole 2D arrays by identifier.

| File | Module | Op |
|------|--------|-----|
| `gemm.v` | `blas_gemm` | `C[N×M] = A[N×K] · B[K×M]` |
| `gemm_bt.v` | `blas_gemm_bt` | `C[N×M] = A[N×K] · Bᵀ` (`B` is `M×K`) |
| `relu.v` | `blas_relu` | elementwise ReLU (clear if MSB set) |
| `row_argmax.v` | `blas_row_argmax` | row-wise one-hot argmax (lowest index on ties) |

Output packed width is `CW` (default `32`). Inputs use `WA`/`WB` (default 8).
`gemm.v` 带 `// @vs` 矩阵视图，可与 `matmul.v` 一样在 vs-view 中核对 `A×B=C`。

Run gemm alone (it is the top when listed first):

```bash
./build/vs run examples/blas/gemm.v --force a_0_0=1 ... --watch c_0_0,...
```
