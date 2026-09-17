#include "vs/ast.h"

#include <string.h>

static void *vs_node_alloc(vs_arena_t *a, size_t size, vs_node_kind_t kind, vs_loc_t loc) {
    vs_node_t *n = vs_arena_alloc(a, size);
    if (!n) {
        return NULL;
    }
    n->kind = kind;
    n->loc = loc;
    n->next = NULL;
    return n;
}

void vs_node_list_append(vs_node_t **list, vs_node_t *n) {
    if (!list || !n) {
        return;
    }
    if (!*list) {
        *list = n;
        return;
    }
    vs_node_t *p = *list;
    while (p->next) {
        p = p->next;
    }
    p->next = n;
}

void vs_expr_list_append(vs_expr_t **list, vs_expr_t *e) {
    vs_node_list_append((vs_node_t **)list, (vs_node_t *)e);
}

vs_design_t *vs_design_new(vs_arena_t *a) {
    return vs_node_alloc(a, sizeof(vs_design_t), VS_DESIGN, (vs_loc_t){0});
}

void vs_design_add_module(vs_design_t *d, vs_module_t *m) {
    if (!d || !m) {
        return;
    }
    vs_node_list_append((vs_node_t **)&d->modules, (vs_node_t *)m);
}

vs_module_t *vs_module_new(vs_arena_t *a, vs_loc_t loc, const char *name) {
    vs_module_t *m = vs_node_alloc(a, sizeof(*m), VS_MODULE, loc);
    if (!m) {
        return NULL;
    }
    m->name = name;
    return m;
}

void vs_module_add_port(vs_module_t *m, vs_port_t *p) {
    if (!m || !p) {
        return;
    }
    vs_node_list_append((vs_node_t **)&m->ports, (vs_node_t *)p);
}

void vs_module_add_item(vs_module_t *m, vs_item_t *it) {
    if (!m || !it) {
        return;
    }
    vs_node_list_append((vs_node_t **)&m->items, (vs_node_t *)it);
}

vs_port_t *vs_port_new(vs_arena_t *a, vs_loc_t loc, vs_port_dir_t dir, vs_range_t *range,
                       const char *name) {
    vs_port_t *p = vs_node_alloc(a, sizeof(*p), VS_PORT, loc);
    if (!p) {
        return NULL;
    }
    p->dir = dir;
    p->range = range;
    p->name = name;
    return p;
}

vs_range_t *vs_range_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *msb, vs_expr_t *lsb) {
    vs_range_t *r = vs_node_alloc(a, sizeof(*r), VS_RANGE, loc);
    if (!r) {
        return NULL;
    }
    r->msb = msb;
    r->lsb = lsb;
    return r;
}

vs_port_decl_t *vs_port_decl_new(vs_arena_t *a, vs_loc_t loc, vs_port_dir_t dir, vs_range_t *range,
                                 vs_expr_t *names) {
    vs_port_decl_t *d = vs_node_alloc(a, sizeof(*d), VS_PORT_DECL, loc);
    if (!d) {
        return NULL;
    }
    d->dir = dir;
    d->range = range;
    d->names = names;
    return d;
}

vs_net_decl_t *vs_net_decl_new(vs_arena_t *a, vs_loc_t loc, vs_range_t *range, vs_expr_t *names) {
    vs_net_decl_t *d = vs_node_alloc(a, sizeof(*d), VS_NET_DECL, loc);
    if (!d) {
        return NULL;
    }
    d->range = range;
    d->names = names;
    return d;
}

vs_reg_decl_t *vs_reg_decl_new(vs_arena_t *a, vs_loc_t loc, vs_range_t *range, vs_expr_t *names) {
    vs_reg_decl_t *d = vs_node_alloc(a, sizeof(*d), VS_REG_DECL, loc);
    if (!d) {
        return NULL;
    }
    d->range = range;
    d->names = names;
    return d;
}

vs_cont_assign_t *vs_cont_assign_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *lhs, vs_expr_t *rhs) {
    vs_cont_assign_t *n = vs_node_alloc(a, sizeof(*n), VS_CONT_ASSIGN, loc);
    if (!n) {
        return NULL;
    }
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

vs_always_t *vs_always_new(vs_arena_t *a, vs_loc_t loc, vs_node_t *event, vs_stmt_t *stmt) {
    vs_always_t *n = vs_node_alloc(a, sizeof(*n), VS_ALWAYS, loc);
    if (!n) {
        return NULL;
    }
    n->event = event;
    n->stmt = stmt;
    return n;
}

vs_initial_t *vs_initial_new(vs_arena_t *a, vs_loc_t loc, vs_stmt_t *stmt) {
    vs_initial_t *n = vs_node_alloc(a, sizeof(*n), VS_INITIAL, loc);
    if (!n) {
        return NULL;
    }
    n->stmt = stmt;
    return n;
}

vs_event_control_t *vs_event_control_new(vs_arena_t *a, vs_loc_t loc, vs_node_t *expr, int star) {
    vs_event_control_t *n = vs_node_alloc(a, sizeof(*n), VS_EVENT_CONTROL, loc);
    if (!n) {
        return NULL;
    }
    n->expr = expr;
    n->star = star;
    return n;
}

vs_event_edge_t *vs_event_edge_new(vs_arena_t *a, vs_loc_t loc, vs_edge_kind_t edge,
                                   const char *name) {
    vs_event_edge_t *n = vs_node_alloc(a, sizeof(*n), VS_EVENT_EDGE, loc);
    if (!n) {
        return NULL;
    }
    n->edge = edge;
    n->name = name;
    return n;
}

vs_event_ident_t *vs_event_ident_new(vs_arena_t *a, vs_loc_t loc, const char *name) {
    vs_event_ident_t *n = vs_node_alloc(a, sizeof(*n), VS_EVENT_IDENT, loc);
    if (!n) {
        return NULL;
    }
    n->name = name;
    return n;
}

vs_block_t *vs_block_new(vs_arena_t *a, vs_loc_t loc, vs_stmt_t *stmts) {
    vs_block_t *n = vs_node_alloc(a, sizeof(*n), VS_BLOCK, loc);
    if (!n) {
        return NULL;
    }
    n->stmts = stmts;
    return n;
}

void vs_block_add_stmt(vs_block_t *b, vs_stmt_t *s) {
    if (!b || !s) {
        return;
    }
    vs_node_list_append((vs_node_t **)&b->stmts, (vs_node_t *)s);
}

vs_if_t *vs_if_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *cond, vs_stmt_t *then_stmt,
                   vs_stmt_t *else_stmt) {
    vs_if_t *n = vs_node_alloc(a, sizeof(*n), VS_IF, loc);
    if (!n) {
        return NULL;
    }
    n->cond = cond;
    n->then_stmt = then_stmt;
    n->else_stmt = else_stmt;
    return n;
}

vs_assign_stmt_t *vs_blocking_assign_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *lhs,
                                         vs_expr_t *rhs) {
    vs_assign_stmt_t *n = vs_node_alloc(a, sizeof(*n), VS_BLOCKING_ASSIGN, loc);
    if (!n) {
        return NULL;
    }
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

vs_assign_stmt_t *vs_nba_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *lhs, vs_expr_t *rhs) {
    vs_assign_stmt_t *n = vs_node_alloc(a, sizeof(*n), VS_NBA, loc);
    if (!n) {
        return NULL;
    }
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

vs_stmt_t *vs_empty_stmt_new(vs_arena_t *a, vs_loc_t loc) {
    return vs_node_alloc(a, sizeof(vs_stmt_t), VS_EMPTY_STMT, loc);
}

vs_expr_t *vs_expr_ident_new(vs_arena_t *a, vs_loc_t loc, const char *name) {
    vs_expr_ident_t *n = vs_node_alloc(a, sizeof(*n), VS_EXPR_IDENT, loc);
    if (!n) {
        return NULL;
    }
    n->name = name;
    return (vs_expr_t *)n;
}

vs_expr_t *vs_expr_number_new(vs_arena_t *a, vs_loc_t loc, const char *text) {
    vs_expr_number_t *n = vs_node_alloc(a, sizeof(*n), VS_EXPR_NUMBER, loc);
    if (!n) {
        return NULL;
    }
    n->text = text;
    return (vs_expr_t *)n;
}

vs_expr_t *vs_expr_unary_new(vs_arena_t *a, vs_loc_t loc, vs_unary_op_t op, vs_expr_t *expr) {
    vs_expr_unary_t *n = vs_node_alloc(a, sizeof(*n), VS_EXPR_UNARY, loc);
    if (!n) {
        return NULL;
    }
    n->op = op;
    n->expr = expr;
    return (vs_expr_t *)n;
}

vs_expr_t *vs_expr_binary_new(vs_arena_t *a, vs_loc_t loc, vs_binary_op_t op, vs_expr_t *lhs,
                              vs_expr_t *rhs) {
    vs_expr_binary_t *n = vs_node_alloc(a, sizeof(*n), VS_EXPR_BINARY, loc);
    if (!n) {
        return NULL;
    }
    n->op = op;
    n->lhs = lhs;
    n->rhs = rhs;
    return (vs_expr_t *)n;
}

vs_expr_t *vs_expr_select_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *expr, vs_expr_t *index) {
    vs_expr_select_t *n = vs_node_alloc(a, sizeof(*n), VS_EXPR_SELECT, loc);
    if (!n) {
        return NULL;
    }
    n->expr = expr;
    n->index = index;
    return (vs_expr_t *)n;
}

vs_expr_t *vs_expr_part_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *expr, vs_expr_t *msb,
                            vs_expr_t *lsb) {
    vs_expr_part_t *n = vs_node_alloc(a, sizeof(*n), VS_EXPR_PART, loc);
    if (!n) {
        return NULL;
    }
    n->expr = expr;
    n->msb = msb;
    n->lsb = lsb;
    return (vs_expr_t *)n;
}

vs_expr_t *vs_expr_concat_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *exprs) {
    vs_expr_concat_t *n = vs_node_alloc(a, sizeof(*n), VS_EXPR_CONCAT, loc);
    if (!n) {
        return NULL;
    }
    n->exprs = exprs;
    return (vs_expr_t *)n;
}
