# vs — Verilog Simulator

从零实现的 **Verilog 模拟器**（解释执行、事件驱动），程序名为 **`vs`**。

当前 **P0** 已具备 flex+bison 前端：`vs --dump-ast` / `--dump-tokens`。

## 特点（目标）

- **纯 C11** 实现
- **解释型**仿真（类似 QEMU 的分阶段、解释执行思路）
- 前端使用 **flex + bison**（macOS 请用 Homebrew `bison` ≥ 3.0）
- 长期向 IEEE 1364 完整语义演进

## 依赖

- CMake ≥ 3.16
- C11 编译器
- flex
- **bison ≥ 3.0**（macOS 系统自带 2.3 不够用）

```bash
brew install bison
# CMake 会通过 /opt/homebrew/opt/bison 找到它
```

## 快速开始

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/vs --dump-ast docs/examples/counter.v
./build/vs --dump-tokens docs/examples/inv.v
ctest --test-dir build --output-on-failure
```

## 文档

- 索引：[`docs/README.md`](docs/README.md)
- Design：[`docs/superpowers/specs/2026-09-17-verilog-emulator-design.md`](docs/superpowers/specs/2026-09-17-verilog-emulator-design.md)
- P0 计划：[`docs/superpowers/plans/2026-09-17-vs-p0-parser.md`](docs/superpowers/plans/2026-09-17-vs-p0-parser.md)

## 状态

**P0（解析 + AST）进行中/基本可用**：空模块、`inv`、计数器 RTL 子集可解析并 dump AST。Elaborate / 仿真见 roadmap。
