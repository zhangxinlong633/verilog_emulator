# 路线图

程序名：**vs**（Verilog Simulator）  
策略：解释执行、事件驱动；前端 flex+bison；语言纯 C11。

## 阶段

### P0 — 解析与 AST（当前）

- [ ] CMake + flex + bison 工程骨架
- [ ] `vs` CLI：`--dump-ast`、`--dump-tokens`、`--help`、`--version`
- [ ] arena / strtab / diag
- [ ] v0.1 文法（见 `grammar-v0.1.md`）
- [ ] AST dump + `tests/parse` 黄金用例
- [ ] 文档（本目录）保持与实现同步

**完成标准：** `ctest` 全绿；能 parse 计数器示例并 dump AST。

### P1 — Elaborate

- 名字绑定（端口、网线、reg、实例）
- 位宽推断/检查
- 层次连接
- 语义诊断（未定义标识符、位宽冲突等）

**完成标准：** 简单计数器 elaborate 成功；故意错误有清晰诊断。

### P2 — 事件驱动仿真

- 4 值逻辑（至少 0/1/x/z）或先 2 值再扩展（实现计划里锁定）
- 敏感列表调度
- 阻塞赋值 vs NBA 语义
- `initial` / `always`
- 基本结束条件（时间上限或 `$finish` 解析+执行）

**完成标准：** 计数器/触发器向量与参考模型（或手算）一致。

### P3+ — 完整度

按优先级递增（可调整）：

1. `#delay`、时间轮细化
2. VCD 导出
3. `$display` / `$monitor` / `$finish`
4. `case`、循环、`parameter`
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
