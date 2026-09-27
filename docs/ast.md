# AST 手册

AST 是 P0 的核心产物，也是 P1/P2 的输入。本文描述节点种类、字段约定与 `--dump-ast` 文本格式。

## 1. 通用约定

### 1.1 节点头

每个节点包含：

```c
typedef struct vs_node {
    vs_node_kind_t kind;
    vs_loc_t       loc;     /* 源位置 */
    /* kind-specific payload */
} vs_node_t;
```

实际可用 tagged union 或「公共头 + 子结构体」两种风格；**推荐**：

```c
struct vs_node {
    vs_node_kind_t kind;
    vs_loc_t       loc;
    struct vs_node *next;   /* 同级列表串接，可选 */
};
```

各 `kind` 对应 `vs_module_t`、`vs_expr_t` 等，经 `VS_NODE(ptr)` 向上转换。

### 1.2 分配

- 一律 `vs_arena_t` 分配
- 字符串来自 `vs_strtab_t`（`const char *` 稳定）
- 列表：单向链表（`next`）或 arena 上的动态数组；**dump 顺序必须稳定**（源码出现序）

### 1.3 位置

`vs_loc_t` 至少包含：`path`、`first_line`、`first_column`、`last_line`、`last_column`。  
dump 默认 **不打印** 位置（避免路径差异导致测试失败）；可用 `--dump-ast --loc`（可选，P0 可后加）调试。

## 2. 节点一览

### 2.1 Design / Module

| Kind | 字段 | 说明 |
|------|------|------|
| `VS_DESIGN` | `modules` | 模块列表 |
| `VS_MODULE` | `name`, `ports`, `items` | 一个 `module` |

### 2.2 声明与端口

| Kind | 字段 | 说明 |
|------|------|------|
| `VS_PORT` | `dir`, `range?`, `name` | 端口列表中的项 |
| `VS_PORT_DECL` | `dir`, `range?`, `names[]` | 模块体内端口声明 |
| `VS_NET_DECL` | `range?`, `names[]` | `wire` |
| `VS_REG_DECL` | `range?`, `names[]` | `reg` |
| `VS_RANGE` | `msb`, `lsb` | 均为表达式 |

方向枚举：`VS_DIR_INPUT` / `OUTPUT` / `INOUT` / `NONE`。

### 2.3 连续赋值与实例

| Kind | 字段 | 说明 |
|------|------|------|
| `VS_CONT_ASSIGN` | `lhs`, `rhs` | `assign` |
| `VS_GENVAR_DECL` | `names` | `genvar` |
| `VS_GENERATE` | `items` | `generate` / `endgenerate` |
| `VS_GEN_FOR` | `init`, `cond`, `step`, `block_name`, `items` | generate `for`；dump 标签 `gen_for <label>` |
| `VS_GEN_IF` | `cond`, then/else items + labels | generate `if`；dump 标签 `gen_if <then>` |
| `VS_GATE_INST` | `gatetype`, `name?`, `terminals[]` | 可选 |
| `VS_MODULE_INST` | `modname`, `instname`, `connections` | P0 可只保留名字与端口连接 AST |

### 2.4 过程块与语句

| Kind | 字段 | 说明 |
|------|------|------|
| `VS_ALWAYS` | `event?`, `stmt` | |
| `VS_INITIAL` | `stmt` | |
| `VS_EVENT_CONTROL` | `expr` | `@(...)` / `@*` |
| `VS_EVENT_EDGE` | `edge`, `name` | posedge/negedge |
| `VS_EVENT_OR` | `list` | `or` 连接的敏感表 |
| `VS_BLOCK` | `stmts[]` | `begin`/`end` |
| `VS_IF` | `cond`, `then_stmt`, `else_stmt?` | |
| `VS_BLOCKING_ASSIGN` | `lhs`, `rhs` | `=` |
| `VS_NBA` | `lhs`, `rhs` | `<=` |
| `VS_TIMING_STMT` | `control`, `stmt` | 语句前的 event control |
| `VS_EMPTY_STMT` | | `;` |

### 2.5 表达式

| Kind | 字段 | 说明 |
|------|------|------|
| `VS_EXPR_IDENT` | `name` | |
| `VS_EXPR_NUMBER` | `text` 或结构化 `{width,base,digits}` | 实现选一种，测试锁定 |
| `VS_EXPR_UNARY` | `op`, `expr` | |
| `VS_EXPR_BINARY` | `op`, `lhs`, `rhs` | |
| `VS_EXPR_SELECT` | `expr`, `index` | `a[i]` |
| `VS_EXPR_PART` | `expr`, `msb`, `lsb` | `a[m:l]` |
| `VS_EXPR_CONCAT` | `exprs[]` | `{a,b}` |

运算符枚举与 dump 用的符号表在实现中集中定义，避免魔法字符散落。

## 3. Dump 格式（黄金测试契约）

### 3.1 原则

- UTF-8 文本，LF 换行
- 缩进：2 空格
- 不输出指针、arena 地址、不稳定时间戳
- 标识符与关键字按源码拼写（关键字小写）
- 子节点顺序 = 源码顺序

### 3.2 示意

源码：

```verilog
module inv(input wire a, output wire y);
  assign y = ~a;
endmodule
```

可能的 dump（示意，实现时以测试文件锁定精确格式）：

```text
design
  module inv
    port input a
    port output y
    assign
      lhs ident y
      rhs unary ~ 
        ident a
```

精确格式在实现 `vs_dump_ast` 时确定，并写入至少一个 `tests/parse/ok/*.ast.expected` 作为 **规范样本**。格式变更必须同步更新全部 expected 文件，并在本节省记变更原因。

## 4. 与仿真的关系（设计约束）

| AST 结构 | 仿真用途 |
|----------|----------|
| `VS_CONT_ASSIGN` | RHS 敏感 → 更新 LHS |
| `VS_ALWAYS` + event | 注册进程与敏感表 |
| `VS_NBA` vs blocking | NBA 进入非阻塞更新区，阻塞立即写 |
| `VS_REG_DECL` / `VS_NET_DECL` | 分配存储 / 网线 |
| `VS_MODULE_INST` | P1 展开层次 |

解析阶段 **不要** 把 `always @(posedge clk) q <= q+1;` 降成自定义「计数器节点」；保持通用语句树。

## 5. API 草图

```c
vs_design_t *vs_parse_files(vs_arena_t *a, vs_diag_t *d,
                            int nfiles, char **paths);

void vs_dump_ast(FILE *out, const vs_design_t *design);
```

CLI `vs --dump-ast file.v` 即调用上述接口。
