# tools/fpga_gemm3/

为 **ASIC 风格**矩阵乘示例（`examples/fpga_gemm3/`）生成叶乘法器集群与分块控制。目录名沿用历史路径。

产品叙事定档 **T=64**（只估算）；本工具生成 **T≤32** 可仿真网表。

## 叶乘法器集群

```bash
python3 tools/fpga_gemm3/gen_mac_array.py --all
```

## 分块（整 tile 并行总线）

```bash
python3 tools/fpga_gemm3/gen_tiled.py --matrix 1024 --tile 32
python3 tools/fpga_gemm3/gen_tiled.py --matrix 8 --tile 4 --demo
```

业务说明、1024→T=64 展开、成本量级见 [`examples/fpga_gemm3/README.md`](../../examples/fpga_gemm3/README.md)。
