# src/

`vs` 可执行文件的 C11 实现。入口：`main.c`。

| 子目录 | 职责 |
|--------|------|
| `front/` | flex/bison 词法语法、AST 构建与 dump |
| `elab/` | 精简 elaborator（符号表、进程扁平化） |
| `sim/` | 事件驱动仿真、NBA、线程池、trace |
| `util/` | arena、strtab、diag |

公开 API 头文件在 `include/vs/`；行为细节以 `docs/` 为准。
