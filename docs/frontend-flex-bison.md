# 前端：flex + bison

`vs` 的解析前端使用 **flex**（词法）与 **bison**（语法）。本文记录工程约定、CMake 集成与排错方法。

## 1. 为什么用生成器

- Verilog 文法大且不规则，长期扩展时，集中维护 `.y` / `.l` 比巨型手写递归下降更易审阅
- 与「纯 C11 仿真内核」不冲突：生成的是 C 代码，运行时无额外语言运行时
- 可重入接口便于将来多文件 / include

## 2. 版本要求

| 工具 | 最低版本 | 说明 |
|------|----------|------|
| flex | 2.6+ | reentrant + bison-bridge |
| bison | 3.0+ | `api.pure full`、`parse.error verbose` |
| C 编译器 | C11 | clang / gcc |

## 3. 文件职责

| 文件 | 职责 |
|------|------|
| `src/front/lex.l` | 关键字、数字、运算符、注释；填充 `yylval` / `yylloc` |
| `src/front/parse.y` | 文法、AST 构建动作、`yyerror` |
| `src/front/ast.c` | 节点构造函数 |
| `src/front/parse_driver.c` | 打开文件、创建 scanner、调用 `yyparse`、销毁 |

生成物（**不进 git**）：

- `lex.yy.c`
- `parse.tab.c` / `parse.tab.h`

## 4. 可重入约定（必须遵守）

### 4.1 bison

```yacc
%define api.pure full
%locations
%define parse.error verbose
%param { yyscan_t scanner }
%parse-param { vs_parse_ctx_t *ctx }
```

`vs_parse_ctx_t` 至少包含：`arena`、`diag`、`design`（或当前 module 栈）、`filename`。

### 4.2 flex

```lex
%option reentrant bison-bridge bison-locations
%option noyywrap nounput noinput
%option prefix="vs_"
```

前缀避免与系统或其他库的 `yylex` 冲突。bison 侧用 `%define api.prefix {vs_}` 保持一致（或同等的符号重命名方案，实现时二选一并写死）。

### 4.3 驱动伪代码

```c
yyscan_t scanner;
vs_lex_init(&scanner);
vs_set_in(fp, scanner);
vs_parse_ctx_t ctx = { .arena = a, .diag = d, ... };
int rc = vs_parse(scanner, &ctx);
vs_lex_destroy(scanner);
```

## 5. %union 与 AST

`%union` 只放：

- `vs_node_t *` / 更细的指针类型
- `const char *`（已 intern）
- 小整数（若需要）

**禁止**在 `%union` 里放大数组或大 struct。所有复杂对象在动作里 `vs_ast_*_new(ctx->arena, ...)`。

## 6. 位置信息

```yacc
%locations
```

lexer 用 `YY_USER_ACTION` 更新 `yylloc` 的列号；换行重置列。  
AST 构造时把 `@$` / `@1` 写入节点 `loc`。

## 7. 错误恢复

- 使用少量 `error` 产生式（例如声明级、语句级），避免无限循环
- 每次恢复前 `yyerrok` 需谨慎；优先报告清晰错误
- `yyerror(YYLTYPE *loc, yyscan_t scanner, vs_parse_ctx_t *ctx, const char *msg)` 转调 `vs_diag_error`

## 8. CMake 集成要点

```cmake
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)
BISON_TARGET(VsParse src/front/parse.y ${CMAKE_CURRENT_BINARY_DIR}/parse.tab.c
             DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parse.tab.h)
FLEX_TARGET(VsLex src/front/lex.l ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)
ADD_FLEX_BISON_DEPENDENCY(VsLex VsParse)
# target_include_directories 加入 BINARY_DIR 与 include/
# add_executable(vs ...)
```

## 9. 调试技巧

1. **只看词法**：`vs --dump-tokens file.v`
2. **bison 冲突**：`bison -Wcounterexamples parse.y`（开发时本地跑），把结论记入 PR/文档
3. **缩小用例**：从 `tests/parse/fail` 最小复现开始
4. **不要手改生成 C 文件**；只改 `.l` / `.y`

## 10. 已知文法风险

- `or`：事件列表分隔符 vs 按位或 → 用不同非终结符隔离
- `<=`：NBA vs 小于等于 → 语句与表达式分轨
- 可选分号/空语句 → 注意 shift/reduce
- ANSI 与非 ANSI 端口混用 → v0.1 限制组合，见 `grammar-v0.1.md`

新增语法时遵循 [contributing-syntax.md](contributing-syntax.md)。
