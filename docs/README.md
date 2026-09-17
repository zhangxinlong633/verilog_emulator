# vs 文档索引

本目录是 **vs（Verilog Simulator）** 的详细文档。实现以这些文档为准；源码头文件只保留简短 API 注释。

可运行示例在仓库根 [`examples/`](../examples/)，不在本目录。代理约束见根目录 [`AGENTS.md`](../AGENTS.md) / [`CLAUDE.md`](../CLAUDE.md)。

## 从哪里读起

| 你想了解… | 阅读 |
|-----------|------|
| 项目要做成什么样 | [superpowers/specs/2026-09-17-verilog-emulator-design.md](superpowers/specs/2026-09-17-verilog-emulator-design.md) |
| 整体分层与数据流 | [architecture.md](architecture.md) |
| P0 支持哪些 Verilog | [grammar-v0.1.md](grammar-v0.1.md) |
| AST 节点与 dump 格式 | [ast.md](ast.md) |
| flex/bison 怎么接 | [frontend-flex-bison.md](frontend-flex-bison.md) |
| 报错格式与退出码 | [diagnostics.md](diagnostics.md) |
| 怎么写解析测试 | [testing.md](testing.md) |
| 版本阶段规划 | [roadmap.md](roadmap.md) |
| 如何新增一条语法 | [contributing-syntax.md](contributing-syntax.md) |
| `@vs` 类型化视图（矩阵等） | [superpowers/specs/2026-09-17-typed-views-anno-design.md](superpowers/specs/2026-09-17-typed-views-anno-design.md) |
| 玩具 NPU Transformer | [superpowers/specs/2026-09-17-npu-transformer-design.md](superpowers/specs/2026-09-17-npu-transformer-design.md) |
| NPU + BLAS 模块实例化 | [superpowers/specs/2026-09-17-npu-blas-hierarchy-design.md](superpowers/specs/2026-09-17-npu-blas-hierarchy-design.md) |
| 参数化 matmul（parameter/for/数组） | [superpowers/specs/2026-09-17-param-matmul-arrays-design.md](superpowers/specs/2026-09-17-param-matmul-arrays-design.md) |
| vs-view 演示截图（GEMM / NPU） | [images/](images/) |

## 演示重点

本仓库用 **可综合风格的 RTL**（非 Python/C 算子）仿真 AI 加速器常见积木：

1. **GEMM** — `examples/blas/gemm.v` / `examples/matmul.v`：组合矩阵乘，黄金 `[[19,22],[43,50]]`。
2. **BLAS 叶子** — `gemm_bt`（Q·Kᵀ）、`relu`、`row_argmax`（硬注意力）。
3. **玩具 NPU** — `examples/npu_transformer.v` 以模块实例化调用上述叶子；时钟 FSM 分 phase 锁存；黄金 `Y=[[3,4],[3,4]]`。

界面截图见 [`images/`](images/)；命令行与说明见根 [`README.md`](../README.md)。
