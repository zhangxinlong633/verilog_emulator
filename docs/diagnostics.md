# 诊断与退出码

## 1. 消息格式

统一格式（与常见编译器接近，便于编辑器跳转）：

```text
<path>:<line>:<column>: <severity>: <message>
```

示例：

```text
tests/parse/fail/bad_assign.v:3:10: error: syntax error, unexpected '=', expecting identifier
```

### 1.1 严重级别

| 级别 | 含义 |
|------|------|
| `error` | 无法产生可信 AST / 不能继续仿真准备 |
| `warning` | 可继续，但可能不符合预期（P0 可少用） |
| `note` | 附加说明，常跟在 error 后 |

P0：lexer/parser 以 `error` 为主。

### 1.2 计数

`vs_diag_t` 维护 `error_count` / `warning_count`。  
任何 `error_count > 0` 时，CLI 退出码为 **1**（除非发生 I/O 等 → **2**）。

## 2. 退出码

| 码 | 场景 |
|----|------|
| 0 | 成功完成请求的动作（如 dump AST）且无 error |
| 1 | 源码词法/语法（及将来语义）错误 |
| 2 | 错误的命令行、文件无法打开、内部断言失败等 |

## 3. 错误恢复策略

目标：**一次运行尽量多报几个独立错误**，但不要在混乱状态下写出「通过测试」的 AST。

规则：

1. 词法非法字符：报告后跳过该字符继续
2. 语法错误：bison `error` 产生式恢复到下一语句/声明边界
3. **黄金成功用例**：要求 `error_count == 0`，才比较 AST dump
4. 若存在 error，`--dump-ast` 仍可打印部分树（调试用），但退出码非 0；CI 成功用例不得依赖残缺树

## 4. API 草图

```c
typedef struct vs_diag vs_diag_t;

void vs_diag_error(vs_diag_t *d, vs_loc_t loc, const char *fmt, ...);
void vs_diag_warn(vs_diag_t *d, vs_loc_t loc, const char *fmt, ...);
void vs_diag_note(vs_diag_t *d, vs_loc_t loc, const char *fmt, ...);

unsigned vs_diag_error_count(const vs_diag_t *d);
void vs_diag_print_all(const vs_diag_t *d, FILE *out);
```

消息最终打印到 **stderr**；AST/token dump 默认 **stdout**，以便重定向与 diff。

## 5. 与 yyerror 的衔接

```c
void yyerror(YYLTYPE *loc, yyscan_t scanner, vs_parse_ctx_t *ctx, const char *msg) {
    (void)scanner;
    vs_diag_error(ctx->diag, yyltype_to_vs_loc(ctx->filename, loc), "%s", msg);
}
```

不要在 `yyerror` 里 `fprintf` 绕过 `vs_diag`，否则测试无法统一捕获。
