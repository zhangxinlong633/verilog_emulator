# examples/

可运行的 Verilog 示例（仓库根目录）。演示用；回归用黄金用例在 `tests/parse/`。

| 文件 | 说明 |
|------|------|
| `inv.v` | 组合反相器 |
| `counter.v` | 4-bit 同步计数器 |
| `alu4.v` | 4-bit ALU（多输入/输出） |
| `combo_bus.v` | 组合多进多出 |
| `sum_1_to_100.v` | 累加 1…100 → `sum=5050` |
| `matmul2x2.v` | 2×2 矩阵乘 → `[[19,22],[43,50]]` |

```bash
./build/vs run examples/counter.v --threads 4 --until 200 \
  --clock clk=10 --reset rst=20 --watch q --trace build/trace.jsonl
```

新增示例时：放在本目录、更新本 README，并视需要加 `tests/sim` / CTest。
