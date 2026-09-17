# 架构

## 1. 设计隐喻：像小型 QEMU

QEMU 把「译码 / 中间表示 / 解释执行 / 加速」分开。`vs` 采用同样思路：

1. **前端**：源码 → 词法 → 语法 → **AST**（不执行）
2. **Elaborate**：AST → 绑定后的层次与网表/IR（不跑时间）
3. **仿真内核**：IR → 事件队列上的解释执行

P0 只实现第 1 步，但 AST 的形状必须能自然接到第 2、3 步，避免「只能漂亮打印」的玩具树。

## 2. 流水线

```text
                    filename(s)
                         │
                         ▼
              ┌──────────────────────┐
              │  vs_compile_file()   │  (P0: parse only)
              │  - open source       │
              │  - create scanner    │
              │  - yyparse           │
              │  - collect diags     │
              └──────────┬───────────┘
                         │  vs_design_t *
                         ▼
              ┌──────────────────────┐
              │  vs_dump_ast()       │  P0 CLI
              └──────────────────────┘
                         │
                         ▼  (P1)
              ┌──────────────────────┐
              │  vs_elaborate()      │
              │  → vs_netlist_t      │
              └──────────┬───────────┘
                         ▼  (P2)
              ┌──────────────────────┐
              │  vs_sim_run()        │
              │  event queue + eval  │
              └──────────────────────┘
```

## 3. 模块边界

| 模块 | 目录 | 职责 | 不得做 |
|------|------|------|--------|
| util | `src/util/` | arena、字符串表、诊断缓冲 | 不认识 Verilog 语法 |
| front | `src/front/` | lex/parse/AST/dump | 不算位宽、不跑仿真 |
| elab | `src/elab/` | 名字解析、位宽、实例展开 | 不推进仿真时间 |
| sim | `src/sim/` | 调度与求值 | 不重新解析源码 |
| cli | `src/main.c` | 参数、调用上述 API | 无业务逻辑 |

公共类型放在 `include/vs/*.h`。内部头文件可用 `src/**/internal.h`。

## 4. 内存模型

- **Arena（区域分配器）**：一次编译一个 arena；AST 全部从中分配；编译结束或 CLI 退出时整块释放。
- **字符串表**：标识符与部分字面量 intern；比较用指针相等。
- **禁止**在 AST 构建路径上无节制 `malloc` 碎分配（诊断消息缓冲除外，可单独管理）。

## 5. 可重入与多文件

flex/bison 使用 **reentrant / pure** 接口，以便：

- 将来实现 `` `include `` 时嵌套解析；
- 并行编译多个互不相关的文件（非 P0 目标，但 API 预留）。

P0 的 `vs` 对每个命令行文件顺序调用编译，合并进同一个 `vs_design_t`（或多个 design——实现时选一种并在 `ast.md` 写死；推荐：**一个进程一次 design，多文件的 module 追加进同一 design**）。

## 6. 与仿真的衔接（前瞻）

Elaborate 之后，过程块应变成：

- 敏感列表（边沿 / 电平 / 隐式 `@*`）
- 语句序列（阻塞赋值 vs NBA 分到不同更新区域）

因此 AST 中 `always` 必须保留 **完整事件表达式** 与 **语句树**，不能在解析期折叠掉。连续赋值 `assign` 作为独立 item，仿真时注册到 RHS 敏感驱动。

## 7. 构建系统

- **CMake** 调用 `flex` / `bison` 生成 `lex.yy.c`、`parse.tab.c`
- 生成物放在 **build 目录**，不进 git
- 产出可执行文件名：**`vs`**
