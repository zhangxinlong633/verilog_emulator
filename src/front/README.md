# src/front/

前端：`lex.l` + `parse.y` + AST。

- `parse_driver.c` — 驱动解析
- `ast.c` / `dump_ast.c` — AST 与 `--dump-ast` / `--dump-tokens`

文法约定见 `docs/grammar-v0.1.md`、`docs/frontend-flex-bison.md`。
