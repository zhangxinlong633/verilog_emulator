# BLAS-style RTL leaves for `vs`

Combinational operators used by `examples/npu_transformer.v`.
Instantiate with named ports; connect whole 2D arrays by identifier.

| File | Module | Op |
|------|--------|-----|
| `gemm.v` | `blas_gemm` | `C[N×M] = A[N×K] · B[K×M]` |
| `gemm_bt.v` | `blas_gemm_bt` | `C[N×M] = A[N×K] · Bᵀ` (`B` is `M×K`) |
| `relu.v` | `blas_relu` | elementwise ReLU (clear if MSB set) |
| `row_argmax.v` | `blas_row_argmax` | row-wise one-hot argmax (lowest index on ties) |

Packed widths: `WA`/`WB` for inputs (default 8), `CW` for gemm outputs (default 32).
ReLU / argmax use a single `W` (default 32).

Run gemm alone (it is the top when listed first):

```bash
./build/vs run examples/blas/gemm.v --force a_0_0=1 ... --watch c_0_0,...
```
