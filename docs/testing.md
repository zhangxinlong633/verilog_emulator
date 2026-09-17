# 测试指南

## 1. 目标

P0 用 **黄金文件** 锁住：

- 合法子集能稳定解析
- AST dump 格式不变温
- 非法输入有可预期的诊断（子串匹配）

## 2. 目录布局

```text
tests/parse/
  ok/
    empty_module.v
    empty_module.ast.expected
    counter.v
    counter.ast.expected
    ...
  fail/
    missing_endmodule.v
    missing_endmodule.diag.expected
    ...
```

## 3. 成功用例（ok）

命令：

```bash
vs --dump-ast path/to/file.v > file.ast.actual
diff -u file.ast.expected file.ast.actual
```

要求：

- 退出码 0
- stderr 无 error（允许空）
- stdout 与 `.ast.expected` 完全一致

CTest 中每个 `ok` 用例注册为一个测试。

## 4. 失败用例（fail）

命令：

```bash
vs --dump-ast path/to/file.v > /dev/null 2> file.diag.actual
# 退出码应为 1
# file.diag.expected 中每一行都应作为子串出现在 actual 中
```

`.diag.expected` 写 **稳定子串**（不要写会随 bison 版本剧烈变化的整句，除非我们已规范化消息）。优先匹配：

- 文件名（或 basename）
- 行号
- 关键词如 `error:`、`unexpected`

## 5. 最低用例清单（P0 必须有）

**ok:**

1. 空模块
2. 非 ANSI 端口 + `wire`/`reg`
3. 简单 `assign`
4. `always @(posedge clk)` + `begin` + NBA
5. `if`/`else`
6. 位选 / 部分选 / 拼接表达式

**fail:**

1. 缺少 `endmodule`
2. 非法 token
3. `assign` 缺分号或残缺表达式

## 6. 如何新增用例

1. 写最小 `.v`
2. 用当前 `vs --dump-ast` 生成 expected（人工审阅后再提交）
3. 更新 `grammar-v0.1.md`（若引入新语法）
4. 注册 CTest

详见 [contributing-syntax.md](contributing-syntax.md)。

## 7. 禁止事项

- 在 expected 里写绝对路径
- 依赖未文档化的 dump 空白差异
- 用「能 parse 大型开源 IP」当 P0 门槛（那是后期目标）
