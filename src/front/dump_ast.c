#include "vs/ast.h"

#include <stdio.h>

static void dump_indent(FILE *out, int depth) {
    for (int i = 0; i < depth; i++) {
        fputs("  ", out);
    }
}

static const char *dir_name(vs_port_dir_t d) {
    switch (d) {
    case VS_DIR_INPUT:
        return "input";
    case VS_DIR_OUTPUT:
        return "output";
    case VS_DIR_INOUT:
        return "inout";
    default:
        return "none";
    }
}

static const char *uop_name(vs_unary_op_t op) {
    switch (op) {
    case VS_UOP_PLUS:
        return "+";
    case VS_UOP_MINUS:
        return "-";
    case VS_UOP_NOT:
        return "~";
    case VS_UOP_LNOT:
        return "!";
    case VS_UOP_AND:
        return "&";
    case VS_UOP_OR:
        return "|";
    case VS_UOP_XOR:
        return "^";
    case VS_UOP_NAND:
        return "~&";
    case VS_UOP_NOR:
        return "~|";
    case VS_UOP_XNOR:
        return "~^";
    default:
        return "?";
    }
}

static const char *bop_name(vs_binary_op_t op) {
    switch (op) {
    case VS_BOP_ADD:
        return "+";
    case VS_BOP_SUB:
        return "-";
    case VS_BOP_MUL:
        return "*";
    case VS_BOP_DIV:
        return "/";
    case VS_BOP_MOD:
        return "%";
    case VS_BOP_AND:
        return "&";
    case VS_BOP_OR:
        return "|";
    case VS_BOP_XOR:
        return "^";
    case VS_BOP_XNOR:
        return "~^";
    case VS_BOP_LAND:
        return "&&";
    case VS_BOP_LOR:
        return "||";
    case VS_BOP_EQ:
        return "==";
    case VS_BOP_NEQ:
        return "!=";
    case VS_BOP_LT:
        return "<";
    case VS_BOP_LE:
        return "<=";
    case VS_BOP_GT:
        return ">";
    case VS_BOP_GE:
        return ">=";
    default:
        return "?";
    }
}

static void dump_expr(FILE *out, const vs_expr_t *e, int depth);
static void dump_stmt(FILE *out, const vs_stmt_t *s, int depth);
static void dump_node(FILE *out, const vs_node_t *n, int depth);

static void dump_expr_list(FILE *out, const vs_expr_t *e, int depth) {
    for (const vs_expr_t *p = e; p; p = (const vs_expr_t *)p->base.next) {
        dump_expr(out, p, depth);
    }
}

static void dump_expr(FILE *out, const vs_expr_t *e, int depth) {
    if (!e) {
        return;
    }
    switch (e->base.kind) {
    case VS_EXPR_IDENT: {
        const vs_expr_ident_t *n = (const vs_expr_ident_t *)e;
        dump_indent(out, depth);
        fprintf(out, "ident %s\n", n->name ? n->name : "");
        break;
    }
    case VS_EXPR_NUMBER: {
        const vs_expr_number_t *n = (const vs_expr_number_t *)e;
        dump_indent(out, depth);
        fprintf(out, "number %s\n", n->text ? n->text : "");
        break;
    }
    case VS_EXPR_UNARY: {
        const vs_expr_unary_t *n = (const vs_expr_unary_t *)e;
        dump_indent(out, depth);
        fprintf(out, "unary %s\n", uop_name(n->op));
        dump_expr(out, n->expr, depth + 1);
        break;
    }
    case VS_EXPR_BINARY: {
        const vs_expr_binary_t *n = (const vs_expr_binary_t *)e;
        dump_indent(out, depth);
        fprintf(out, "binary %s\n", bop_name(n->op));
        dump_expr(out, n->lhs, depth + 1);
        dump_expr(out, n->rhs, depth + 1);
        break;
    }
    case VS_EXPR_SELECT: {
        const vs_expr_select_t *n = (const vs_expr_select_t *)e;
        dump_indent(out, depth);
        fputs("select\n", out);
        dump_expr(out, n->expr, depth + 1);
        dump_expr(out, n->index, depth + 1);
        break;
    }
    case VS_EXPR_INDEX: {
        const vs_expr_index_t *n = (const vs_expr_index_t *)e;
        dump_indent(out, depth);
        fputs("index\n", out);
        dump_expr(out, n->expr, depth + 1);
        dump_expr(out, n->index, depth + 1);
        break;
    }
    case VS_EXPR_PART: {
        const vs_expr_part_t *n = (const vs_expr_part_t *)e;
        dump_indent(out, depth);
        fputs("part\n", out);
        dump_expr(out, n->expr, depth + 1);
        dump_expr(out, n->msb, depth + 1);
        dump_expr(out, n->lsb, depth + 1);
        break;
    }
    case VS_EXPR_CONCAT: {
        const vs_expr_concat_t *n = (const vs_expr_concat_t *)e;
        dump_indent(out, depth);
        fputs("concat\n", out);
        dump_expr_list(out, n->exprs, depth + 1);
        break;
    }
    default:
        dump_indent(out, depth);
        fprintf(out, "expr_kind_%d\n", (int)e->base.kind);
        break;
    }
}

static void dump_range(FILE *out, const vs_range_t *r, int depth) {
    for (const vs_range_t *cur = r; cur; cur = (const vs_range_t *)cur->base.next) {
        dump_indent(out, depth);
        fputs("range\n", out);
        dump_expr(out, cur->msb, depth + 1);
        dump_expr(out, cur->lsb, depth + 1);
    }
}

static void dump_event(FILE *out, const vs_node_t *n, int depth) {
    if (!n) {
        return;
    }
    if (n->kind == VS_EVENT_CONTROL) {
        const vs_event_control_t *ec = (const vs_event_control_t *)n;
        dump_indent(out, depth);
        fputs("event_control\n", out);
        if (ec->star) {
            dump_indent(out, depth + 1);
            fputs("star\n", out);
        } else {
            dump_event(out, ec->expr, depth + 1);
        }
        return;
    }
    if (n->kind == VS_EVENT_EDGE) {
        const vs_event_edge_t *e = (const vs_event_edge_t *)n;
        dump_indent(out, depth);
        fprintf(out, "edge %s %s\n", e->edge == VS_EDGE_POSEDGE ? "posedge" : "negedge",
                e->name ? e->name : "");
        if (n->next) {
            dump_event(out, n->next, depth);
        }
        return;
    }
    if (n->kind == VS_EVENT_IDENT) {
        const vs_event_ident_t *e = (const vs_event_ident_t *)n;
        dump_indent(out, depth);
        fprintf(out, "event_ident %s\n", e->name ? e->name : "");
        if (n->next) {
            dump_event(out, n->next, depth);
        }
        return;
    }
    /* OR-list: linked via next */
    dump_node(out, n, depth);
}

static void dump_stmt(FILE *out, const vs_stmt_t *s, int depth) {
    if (!s) {
        return;
    }
    switch (s->base.kind) {
    case VS_BLOCK: {
        const vs_block_t *b = (const vs_block_t *)s;
        dump_indent(out, depth);
        fputs("block\n", out);
        for (const vs_stmt_t *p = b->stmts; p; p = (const vs_stmt_t *)p->base.next) {
            dump_stmt(out, p, depth + 1);
        }
        break;
    }
    case VS_IF: {
        const vs_if_t *n = (const vs_if_t *)s;
        dump_indent(out, depth);
        fputs("if\n", out);
        dump_indent(out, depth + 1);
        fputs("cond\n", out);
        dump_expr(out, n->cond, depth + 2);
        dump_indent(out, depth + 1);
        fputs("then\n", out);
        dump_stmt(out, n->then_stmt, depth + 2);
        if (n->else_stmt) {
            dump_indent(out, depth + 1);
            fputs("else\n", out);
            dump_stmt(out, n->else_stmt, depth + 2);
        }
        break;
    }
    case VS_FOR: {
        const vs_for_t *n = (const vs_for_t *)s;
        dump_indent(out, depth);
        fputs("for\n", out);
        dump_indent(out, depth + 1);
        fputs("init\n", out);
        dump_stmt(out, n->init, depth + 2);
        dump_indent(out, depth + 1);
        fputs("cond\n", out);
        dump_expr(out, n->cond, depth + 2);
        dump_indent(out, depth + 1);
        fputs("step\n", out);
        dump_stmt(out, n->step, depth + 2);
        dump_indent(out, depth + 1);
        fputs("body\n", out);
        dump_stmt(out, n->body, depth + 2);
        break;
    }
    case VS_BLOCKING_ASSIGN: {
        const vs_assign_stmt_t *n = (const vs_assign_stmt_t *)s;
        dump_indent(out, depth);
        fputs("blocking\n", out);
        dump_indent(out, depth + 1);
        fputs("lhs\n", out);
        dump_expr(out, n->lhs, depth + 2);
        dump_indent(out, depth + 1);
        fputs("rhs\n", out);
        dump_expr(out, n->rhs, depth + 2);
        break;
    }
    case VS_NBA: {
        const vs_assign_stmt_t *n = (const vs_assign_stmt_t *)s;
        dump_indent(out, depth);
        fputs("nba\n", out);
        dump_indent(out, depth + 1);
        fputs("lhs\n", out);
        dump_expr(out, n->lhs, depth + 2);
        dump_indent(out, depth + 1);
        fputs("rhs\n", out);
        dump_expr(out, n->rhs, depth + 2);
        break;
    }
    case VS_EMPTY_STMT:
        dump_indent(out, depth);
        fputs("empty\n", out);
        break;
    case VS_EVENT_CONTROL: {
        dump_event(out, (const vs_node_t *)s, depth);
        break;
    }
    default:
        dump_indent(out, depth);
        fprintf(out, "stmt_kind_%d\n", (int)s->base.kind);
        break;
    }
}

static void dump_item(FILE *out, const vs_item_t *it, int depth) {
    if (!it) {
        return;
    }
    switch (it->base.kind) {
    case VS_PORT_DECL: {
        const vs_port_decl_t *d = (const vs_port_decl_t *)it;
        dump_indent(out, depth);
        fprintf(out, "port_decl %s\n", dir_name(d->dir));
        dump_range(out, d->range, depth + 1);
        dump_expr_list(out, d->names, depth + 1);
        break;
    }
    case VS_NET_DECL: {
        const vs_net_decl_t *d = (const vs_net_decl_t *)it;
        dump_indent(out, depth);
        fputs("net_decl\n", out);
        dump_range(out, d->range, depth + 1);
        if (d->unpacked_dims) {
            dump_indent(out, depth + 1);
            fputs("unpacked\n", out);
            dump_range(out, d->unpacked_dims, depth + 2);
        }
        dump_expr_list(out, d->names, depth + 1);
        break;
    }
    case VS_REG_DECL: {
        const vs_reg_decl_t *d = (const vs_reg_decl_t *)it;
        dump_indent(out, depth);
        fputs("reg_decl\n", out);
        dump_range(out, d->range, depth + 1);
        if (d->unpacked_dims) {
            dump_indent(out, depth + 1);
            fputs("unpacked\n", out);
            dump_range(out, d->unpacked_dims, depth + 2);
        }
        dump_expr_list(out, d->names, depth + 1);
        break;
    }
    case VS_INTEGER_DECL: {
        const vs_integer_decl_t *d = (const vs_integer_decl_t *)it;
        dump_indent(out, depth);
        fputs("integer_decl\n", out);
        dump_expr_list(out, d->names, depth + 1);
        break;
    }
    case VS_CONT_ASSIGN: {
        const vs_cont_assign_t *n = (const vs_cont_assign_t *)it;
        dump_indent(out, depth);
        fputs("assign\n", out);
        dump_indent(out, depth + 1);
        fputs("lhs\n", out);
        dump_expr(out, n->lhs, depth + 2);
        dump_indent(out, depth + 1);
        fputs("rhs\n", out);
        dump_expr(out, n->rhs, depth + 2);
        break;
    }
    case VS_INSTANCE: {
        const vs_instance_t *n = (const vs_instance_t *)it;
        dump_indent(out, depth);
        fprintf(out, "instance %s %s\n", n->mod_name ? n->mod_name : "",
                n->inst_name ? n->inst_name : "");
        for (const vs_named_conn_t *c = n->params; c; c = (const vs_named_conn_t *)c->base.next) {
            dump_indent(out, depth + 1);
            fprintf(out, "param .%s\n", c->name ? c->name : "");
            dump_expr(out, c->expr, depth + 2);
        }
        for (const vs_named_conn_t *c = n->ports; c; c = (const vs_named_conn_t *)c->base.next) {
            dump_indent(out, depth + 1);
            fprintf(out, "port .%s\n", c->name ? c->name : "");
            dump_expr(out, c->expr, depth + 2);
        }
        break;
    }
    case VS_ALWAYS: {
        const vs_always_t *n = (const vs_always_t *)it;
        dump_indent(out, depth);
        fputs("always\n", out);
        if (n->event) {
            dump_event(out, n->event, depth + 1);
        }
        dump_stmt(out, n->stmt, depth + 1);
        break;
    }
    case VS_INITIAL: {
        const vs_initial_t *n = (const vs_initial_t *)it;
        dump_indent(out, depth);
        fputs("initial\n", out);
        dump_stmt(out, n->stmt, depth + 1);
        break;
    }
    default:
        dump_indent(out, depth);
        fprintf(out, "item_kind_%d\n", (int)it->base.kind);
        break;
    }
}

static void dump_node(FILE *out, const vs_node_t *n, int depth) {
    (void)out;
    (void)n;
    (void)depth;
}

void vs_dump_ast(FILE *out, const vs_design_t *design) {
    if (!out || !design) {
        return;
    }
    fputs("design\n", out);
    for (const vs_module_t *m = design->modules; m; m = (const vs_module_t *)m->base.next) {
        dump_indent(out, 1);
        fprintf(out, "module %s\n", m->name ? m->name : "");
        for (const vs_param_decl_t *p = m->params; p; p = (const vs_param_decl_t *)p->base.next) {
            dump_indent(out, 2);
            fprintf(out, "param %s\n", p->name ? p->name : "");
            dump_expr(out, p->value, 3);
        }
        for (const vs_port_t *p = m->ports; p; p = (const vs_port_t *)p->base.next) {
            dump_indent(out, 2);
            fprintf(out, "port %s %s\n", dir_name(p->dir), p->name ? p->name : "");
            dump_range(out, p->range, 3);
        }
        for (const vs_item_t *it = m->items; it; it = (const vs_item_t *)it->base.next) {
            dump_item(out, it, 2);
        }
    }
}
