# vs — Verilog Simulator

从零实现的 **Verilog 模拟器**（解释执行、事件驱动），程序名为 **`vs`**。

当前阶段（**P0**）优先把 **flex + bison 前端与 AST** 做扎实；仿真内核在后续阶段加入。

## 特点（目标）

- **纯 C11** 实现
- **解释型**仿真（类似 QEMU 的分阶段、解释执行思路，而非 Verilator 式编译仿真）
- 前端使用成熟的 **flex + bison**
- 长期向 IEEE 1364 完整语义演进

## 快速开始（P0 就绪后）

```bash
# 依赖: cmake, c11 compiler, flex, bison (>= 3.0)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/vs --dump-ast examples/counter.v
ctest --test-dir build --output-on-failure
```

## 文档

详细设计与手册见 [`docs/README.md`](docs/README.md)。  
正式 design spec：[`docs/superpowers/specs/2026-09-17-verilog-emulator-design.md`](docs/superpowers/specs/2026-09-17-verilog-emulator-design.md)。

## 状态

仓库处于设计落地阶段：文档先行，实现按 roadmap 推进。
