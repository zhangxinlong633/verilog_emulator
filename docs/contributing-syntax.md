# 如何新增一条语法

面向贡献者：把某种 Verilog 结构从「不支持」做到「可解析 + 可测试 + 有文档」。

## 步骤清单

### 1. 更新语言文档

编辑 `docs/grammar-v0.1.md`：

- 在支持表中增加条目
- 补充 BNF 片段
- 加一个最小示例

若该特性属于 P1/P2 语义、仅 P0 解析，在文档中标明「parse-only」。

### 2. 词法（如需要）

编辑 `src/front/lex.l`：

- 新关键字：加到关键字表，返回对应 token
- 新运算符：注意与已有规则的最长匹配（flex 规则顺序）

### 3. 语法与 AST

编辑 `src/front/parse.y`：

- 声明 token / 非终结符类型
- 写产生式，动作里调用 `vs_ast_*_new`
- 检查 shift/reduce、reduce/reduce；用 `bison -Wcounterexamples`

编辑 `include/vs/ast.h` 与 `src/front/ast.c`：

- 新 `vs_node_kind_t`
- 构造函数与必要的 accessor

编辑 `src/front/dump_ast.c`：

- 为新节点增加稳定 dump

### 4. 测试

1. 在 `tests/parse/ok/` 添加最小 `.v`
2. 生成并审阅 `.ast.expected`
3. 若是错误路径，放到 `tests/parse/fail/` 并写 `.diag.expected`
4. 注册 CTest（或加入现有 glob 规则）

### 5. 诊断文案

若需要专用错误消息，走 `vs_diag_*`，并更新 `docs/diagnostics.md` 中的约定（如有新严重级别或格式）。

### 6. 自检

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

确认：

- [ ] 旧用例未碎
- [ ] 新用例通过
- [ ] `docs/ast.md` 节点表已更新
- [ ] `docs/roadmap.md` 若关闭某项 checklist 则勾选

## 评审关注点

- 是否引入与 `or` / `<=` 相关的新歧义
- dump 是否稳定、无绝对路径
- 节点是否保留仿真需要的信息（不要过早降糖）

## 示例：加入 `parameter`（假想）

1. grammar 文档增加 `parameter` 声明  
2. lex 关键字 `parameter`  
3. parse：`parameter id = expr ;` → `VS_PARAM_DECL`  
4. dump：`param FOO = ...`  
5. ok 测试：`module m; parameter W = 8; endmodule`  
6. P1 再做常量折叠与位宽使用  

P0 若暂不实现，不要只改文档不改测试——要么实现，要么在 grammar 的非目标表保留。
