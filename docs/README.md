# vs 文档索引

本目录是 **vs（Verilog Simulator）** 的详细文档。实现以这些文档为准；源码头文件只保留简短 API 注释。

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

## 示例源码

`docs/examples/` 下放置说明用的 `.v` 片段（与 `tests/parse` 黄金用例互补：前者重讲解，后者重回归）。
