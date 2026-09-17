# 路线图

程序名：**vs**（Verilog Simulator）  
策略：解释执行、事件驱动；前端 flex+bison；语言纯 C11。

## 阶段

### P0 — 解析与 AST

- [x] CMake + flex + bison 工程骨架
- [x] `vs` CLI：`--dump-ast`、`--dump-tokens`、`--help`、`--version`
- [x] arena / strtab / diag
- [x] v0.1 文法核心（见 `grammar-v0.1.md`；仍可扩展）
- [x] AST dump + `tests/parse` 黄金用例（empty / inv / counter + fail）
- [x] 文档（本目录）与实现同步中

**完成标准：** `ctest` 全绿；能 parse 计数器示例并 dump AST。 — **已达到**

### P2-lite-MT — 最小多线程仿真 + Trace + Node 视图（当前）

- [x] Flat elab-lite（单模块符号表 / process）
- [x] 事件调度 + NBA + pthread 工作池（`--threads`）
- [x] `--trace` JSONL、`--clock` / `--reset` / `--watch`、`-v`
- [x] `tools/vs-view`（**TypeScript**）时间线 + 精简波形
- [x] `// @vs` 注解 → meta `views`/`ops`/`exprs` + vs-view 矩阵 Typed views
- [x] `parameter` / 过程 `for` / 1D·2D unpacked（元素名 `base_i` / `base_i_j`）+ `examples/matmul.v`
- [x] `@vs array=` 视图按参数展开 cells
- [x] 确定性测试：threads 1 vs 4 最终 watch 一致

**完成标准：** counter 可 `vs run`；Node UI 可查看 trace。 — **已达到**

### P1 — Elaborate（完整）

- 名字绑定（端口、网线、reg、实例）
- 位宽推断/检查
- 层次连接
- 语义诊断（未定义标识符、位宽冲突等）

**完成标准：** 简单计数器 elaborate 成功；故意错误有清晰诊断。

### P2 — 事件驱动仿真（完整）

- 完整 4 值逻辑、更丰富过程语义
- 通用 `#delay` / `$finish` 等

**完成标准：** 更广 RTL 子集与参考模型一致。

### P3+ — 完整度

按优先级递增（可调整）：

1. `#delay`、时间轮细化
2. VCD 导出
3. `$display` / `$monitor` / `$finish`
4. `case`、`generate`、更深数组等（`parameter`/`for`/1D·2D 已在 P2-lite）
5. 简单预处理
6. 更多网型、UDP、specify…
7. （可选）加速路径；默认仍解释执行

## 非目标（长期仍可能不做）

- 完整 SystemVerilog
- 综合 / 形式验证
- GUI 波形查看器（可用外部 GTKWave + VCD）
- 替换商业仿真器的全部性能优化

## 原则

1. **文档先行或同步**：语法变更必改 `grammar-v0.1.md` 与测试
2. **子集正确优于全集残缺**
3. **AST 保持可仿真**，不做过度降糖
