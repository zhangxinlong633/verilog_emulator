# AGENTS.md

面向 Cursor / Codex / Claude 等代理的仓库约束。与 `CLAUDE.md` 保持同步；冲突时以本文件与用户当前指令为准。

## 目录约定

| 路径 | 用途 |
|------|------|
| `examples/` | **所有**可演示 / 可 `vs run` 的 Verilog 示例（仓库根，不在 `docs/` 下） |
| `docs/` | 设计与说明文档（不含示例 `.v`） |
| `src/` | C11 实现 |
| `include/vs/` | 公共头文件 |
| `tests/` | 回归测试（解析黄金用例、仿真脚本） |
| `tools/` | 配套工具（如 `vs-view`） |

禁止把新示例写回 `docs/examples/`（该路径已废弃）。

## README 约定

- **每个有意义的目录**必须有 `README.md`（至少说明职责与关键入口）。
- 新增顶层目录或主要子目录时，**同时**补 README。
- 改目录职责或移动文件时，**同步**更新对应 README 与根 `README.md` 中的路径。

## 示例与测试

- 演示：`examples/*.v` + 更新 `examples/README.md`。
- 回归：`tests/parse` 或 `tests/sim`；仿真测试的 `SRC` 指向 `examples/`。
- 文档里的命令路径一律写 `examples/...`，不要写 `docs/examples/...`。
- 业务语义展示用 `// @vs view|op|expr` 写在示例源码里；由 C 预扫描进 `meta`，vs-view 渲染。不要把展示类型硬编码进 UI。
- unpacked 数组元素对外名：`base_i` / `base_i_j`（十进制）；`@vs view matrix … array=a rows=N cols=K` 在 elab 后展开为这些名字。

## 代码风格（摘要）

- 语言：C11；仿真为解释、事件驱动。
- 程序名：`vs`。
- 详细设计以 `docs/` 与 `docs/superpowers/specs/` 为准；头文件只留短 API 注释。
- 不要主动创建用户未要求的 markdown，**本条约定要求的 README / AGENTS / CLAUDE 除外**。
