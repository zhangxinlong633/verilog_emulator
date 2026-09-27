# Verilog 语法子集 v0.1

本文定义 **P0 解析器必须接受** 的语言范围，以及明确推迟的特性。  
目标不是完整 IEEE 1364，而是支撑后续「最小 RTL 仿真」（`wire`/`reg`、`assign`、边沿 `always`、阻塞/非阻塞赋值）。

## 1. 总览

| 类别 | v0.1 | 备注 |
|------|------|------|
| 模块与端口 | 是 | |
| `wire` / `reg` + 位宽 | 是 | |
| `assign` | 是 | |
| `always` / `initial` | 是 | |
| `begin`/`end`, `if`/`else` | 是 | |
| 阻塞 `=` / 非阻塞 `<=` | 是 | |
| 边沿/电平事件控制 | 是 | |
| `parameter` / `integer` / `for` | 是 | 见下文 |
| `generate` `for` / `if` | 是 | `genvar` + `generate`/`endgenerate`；elab 展开。无 `generate case` |
| 1D / 2D unpacked 数组 | 是 | 元素公开名 `base_i` / `base_i_j`（十进制下标） |
| 模块实例化（命名端口） | 是 | elab 展平；多文件 `vs run`；无位置端口 |
| 常用表达式 | 是 | |
| 预处理 | **否** | |
| `case` / `while` / task/function | **否** | |
| `#delay` | **否** | P3+ |
| 系统任务执行 | **否** | 可后加解析 |

## 2. 词法

### 2.1 空白与注释

- 空白：空格、tab、换行
- 注释：`//` 行注释、`/* ... */` 块注释（块注释不嵌套）

### 2.2 标识符

- 以字母或 `_` 开头，后接字母、数字、`$`、`_`
- 转义标识符（`\foo `）**v0.1 不做**

### 2.3 关键字（v0.1）

```text
module endmodule
input output inout
wire reg
parameter integer
genvar generate endgenerate
assign
always initial
begin end
if else for
posedge negedge
or              // 仅在事件列表中作为分隔语义；作为运算符另见表达式
```

说明：`or` 在 Verilog 事件列表里是敏感列表分隔符；在表达式里是按位或。bison 中需按上下文区分（事件列表用专用非终结符）。

### 2.4 数字

支持：

- 无进制：`42`
- 带位宽与进制：`8'b1010_1010`、`16'hcafe`、`4'd10`、`3'o7`
- 下划线可出现在数字中（词法阶段丢弃）

暂不支持：`x`/`z` 在数值中的完整 4 值语义可先作为字面量字符保留，**求值**留到仿真期；P0 dump 原样保留文本或规范化内部表示（实现时在 `ast.md` 固定一种）。

### 2.5 运算符与分隔符（词法）

```text
( ) [ ] { } , ; . @ * 
= <=
+ - * / %
& | ^ ~ 
&& ||
== != < > <= >=
~& ~| ~^ ^~     // 可列入词法；缩减运算表达式可后开
```

注意：`<=` 既是关系运算符也可能是非阻塞赋值。**赋值与表达式中的 `<=` 由语法上下文区分**（语句 vs 表达式）。

## 3. 语法（描述性 BNF）

下列 BNF 为指导性描述，以实现仓库中 `parse.y` 为准；冲突解决策略写在 `frontend-flex-bison.md`。

```text
design           ::= module_decl+

module_decl      ::= 'module' id param_port_list? port_list? ';' module_item* 'endmodule'

param_port_list  ::= '#' '(' param_port_item (',' param_port_item)* ')'
param_port_item  ::= 'parameter'? id '=' const_expr

port_list        ::= '(' port_list_items? ')'
port_list_items  ::= port (',' port)*
port             ::= port_direction ('wire'|'reg')? range? id unpacked_dims?
port_direction   ::= 'input' | 'output' | 'inout'

module_item      ::= parameter_decl
                   | integer_decl
                   | genvar_decl
                   | generate_region
                   | port_decl
                   | net_decl
                   | reg_decl
                   | cont_assign
                   | always_construct
                   | initial_construct
                   | gate_instantiation      // 未实现
                   | module_instantiation

module_instantiation ::= id param_override? id '(' named_port_conn_list? ')' ';'
param_override       ::= '#' '(' named_conn_list ')'
named_conn_list      ::= named_conn (',' named_conn)*
named_conn           ::= '.' id '(' expr ')'
named_port_conn_list ::= named_conn_list
/* v1: 仅命名端口；整数组连接用简单标识符；elab 展平为 netlist（inst_ 前缀） */

parameter_decl   ::= 'parameter' id '=' const_expr ';'
integer_decl     ::= 'integer' list_of_ids ';'
genvar_decl      ::= 'genvar' list_of_ids ';'
generate_region  ::= 'generate' gen_item* 'endgenerate'
gen_item         ::= module_item | gen_for | gen_if
gen_for          ::= 'for' '(' genvar '=' const ';' const ';' genvar '=' const ')' gen_block
gen_if           ::= 'if' '(' const ')' gen_block ( 'else' gen_block )?
gen_block        ::= 'begin' ( ':' id )? gen_item* 'end'
/* elab: genvar 循环展开为具体 assign/实例/声明。
   带局部声明的块必须有标签；公开名 block_index_name（if 块为 block_name）。
   单次 generate for 上限 65536 次。无 generate case。 */
port_decl        ::= port_direction range? list_of_ids ';'
net_decl         ::= 'wire' range? list_of_ids ';'
                   | 'wire' range? id unpacked_dims ';'
reg_decl         ::= 'reg' range? list_of_ids ';'
                   | 'reg' range? id unpacked_dims ';'
range            ::= '[' const_expr ':' const_expr ']'
unpacked_dims    ::= range range?  /* 1D → base_i；2D → base_i_j（row-major） */
/* const_expr (elab): number | parameter id | unary +/- | binary + - * / */


cont_assign      ::= 'assign' lvalue '=' expr ';'

always_construct ::= 'always' event_control? statement
initial_construct::= 'initial' statement

event_control    ::= '@' '*' 
                   | '@' '(' event_expr ')'
event_expr       ::= event_expr_term ('or' event_expr_term)*
event_expr_term  ::= 'posedge' id
                   | 'negedge' id
                   | id
                   | '*'                    // 与 @* 等价形式之一

statement        ::= lvalue '=' expr ';'
                   | lvalue '<=' expr ';'
                   | 'begin' statement* 'end'
                   | 'if' '(' expr ')' statement ('else' statement)?
                   | 'for' '(' lvalue '=' expr ';' expr ';' lvalue '=' expr ')' statement
                   | event_control statement
                   | ';'

lvalue           ::= id
                   | id '[' expr ']'                 /* 1D 索引或位选 */
                   | id '[' expr ']' '[' expr ']'    /* 2D 数组索引 */
                   | id '[' expr ':' expr ']'
                   | '{' lvalue (',' lvalue)* '}'   // 可选：拼接左值可后开

expr             ::= primary
                   | unary_op expr
                   | expr binary_op expr
                   | '{' expr (',' expr)* '}'

primary          ::= number | id
                   | primary '[' expr ']'            /* 可链式：a[i][j] */
                   | id '[' expr ':' expr ']'
                   | '(' expr ')'
```

### 3.1 端口风格

v0.1 **同时允许**：

1. ANSI 风格：`module m(input wire [7:0] a, output reg [7:0] y);`
2. 非 ANSI：`module m(a, y); input [7:0] a; output [7:0] y; ...`

若 bison 冲突过大，可 **先实现非 ANSI + 简单 ANSI（方向写在端口列表）**，并在本文记录实际支持矩阵。以测试用例为准。

## 4. 示例（说明用）

### 4.1 空模块

```verilog
module empty;
endmodule
```

### 4.2 组合 assign

```verilog
module inv(input wire a, output wire y);
  assign y = ~a;
endmodule
```

### 4.3 边沿触发计数器（P2 仿真目标；P0 必须能 parse）

```verilog
module counter(input wire clk, input wire rst, output reg [3:0] q);
  always @(posedge clk) begin
    if (rst)
      q <= 4'd0;
    else
      q <= q + 4'd1;
  end
endmodule
```

更多可运行于测试的文件见 `tests/parse/ok/`（实现后添加）。

## 5. 明确非目标（v0.1）

- 预处理与宏
- `localparam`；`parameter` **已支持**（模块头 `#(...)` 与 body `parameter`；elab 常量折叠进 packed/unpacked range）。`parameter` 不能写在 `generate` 内部
- `case`/`casex`/`casez`（含 `generate case`）
- `while`/`repeat`/`forever`；过程 `for` **已支持**（迭代上限防护）
- unpacked 数组更深于 2D
- `generate for` / `generate if` **已支持**（必须包在 `generate`/`endgenerate` 里；见上文）
- 1D/2D unpacked **已支持**（元素公开名 `base_i` / `base_i_j`；ANSI 端口可带 unpacked dims）
- `task`/`function`
- 指定块 `specify`、UDP、`tri*` 等网型
- 属性 `(* ... *)`
- SystemVerilog 全部

`// @vs view matrix` 支持显式 `cells=`，或 `array=base rows=N cols=K`（`rows`/`cols` 可为字面量或 parameter 名；elab 后展开为 `base_i_j`）。