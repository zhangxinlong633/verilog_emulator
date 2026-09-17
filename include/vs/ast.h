#ifndef VS_AST_H
#define VS_AST_H

#include "vs/arena.h"
#include "vs/diag.h"

#include <stdio.h>

typedef enum vs_node_kind {
    VS_DESIGN = 1,
    VS_MODULE,
    VS_PORT,
    VS_PORT_DECL,
    VS_NET_DECL,
    VS_REG_DECL,
    VS_RANGE,
    VS_CONT_ASSIGN,
    VS_ALWAYS,
    VS_INITIAL,
    VS_EVENT_CONTROL,
    VS_EVENT_EDGE,
    VS_EVENT_IDENT,
    VS_EVENT_OR,
    VS_BLOCK,
    VS_IF,
    VS_BLOCKING_ASSIGN,
    VS_NBA,
    VS_EMPTY_STMT,
    VS_EXPR_IDENT,
    VS_EXPR_NUMBER,
    VS_EXPR_UNARY,
    VS_EXPR_BINARY,
    VS_EXPR_SELECT,
    VS_EXPR_PART,
    VS_EXPR_CONCAT
} vs_node_kind_t;

typedef enum vs_port_dir {
    VS_DIR_NONE = 0,
    VS_DIR_INPUT,
    VS_DIR_OUTPUT,
    VS_DIR_INOUT
} vs_port_dir_t;

typedef enum vs_edge_kind {
    VS_EDGE_NONE = 0,
    VS_EDGE_POSEDGE,
    VS_EDGE_NEGEDGE
} vs_edge_kind_t;

typedef enum vs_unary_op {
    VS_UOP_PLUS = 1,
    VS_UOP_MINUS,
    VS_UOP_NOT,  /* ~ */
    VS_UOP_LNOT, /* ! */
    VS_UOP_AND,
    VS_UOP_OR,
    VS_UOP_XOR,
    VS_UOP_NAND,
    VS_UOP_NOR,
    VS_UOP_XNOR
} vs_unary_op_t;

typedef enum vs_binary_op {
    VS_BOP_ADD = 1,
    VS_BOP_SUB,
    VS_BOP_MUL,
    VS_BOP_DIV,
    VS_BOP_MOD,
    VS_BOP_AND,
    VS_BOP_OR,
    VS_BOP_XOR,
    VS_BOP_XNOR,
    VS_BOP_LAND,
    VS_BOP_LOR,
    VS_BOP_EQ,
    VS_BOP_NEQ,
    VS_BOP_LT,
    VS_BOP_LE,
    VS_BOP_GT,
    VS_BOP_GE
} vs_binary_op_t;

typedef struct vs_node vs_node_t;
typedef struct vs_design vs_design_t;
typedef struct vs_module vs_module_t;
typedef struct vs_port vs_port_t;
typedef struct vs_range vs_range_t;
typedef struct vs_expr vs_expr_t;
typedef struct vs_stmt vs_stmt_t;
typedef struct vs_item vs_item_t;

struct vs_node {
    vs_node_kind_t kind;
    vs_loc_t loc;
    vs_node_t *next;
};

typedef struct vs_anno_set vs_anno_set_t;

struct vs_design {
    vs_node_t base;
    vs_module_t *modules; /* linked via base.next cast */
    const vs_anno_set_t *annos; /* optional // @vs metadata */
};

struct vs_module {
    vs_node_t base;
    const char *name;
    vs_port_t *ports;
    vs_item_t *items;
};

struct vs_port {
    vs_node_t base;
    vs_port_dir_t dir;
    vs_range_t *range;
    const char *name;
};

struct vs_range {
    vs_node_t base;
    vs_expr_t *msb;
    vs_expr_t *lsb;
};

/* Generic module item / statement / expression wrappers share vs_node header. */
struct vs_item {
    vs_node_t base;
};

struct vs_stmt {
    vs_node_t base;
};

struct vs_expr {
    vs_node_t base;
};

typedef struct vs_port_decl {
    vs_node_t base;
    vs_port_dir_t dir;
    vs_range_t *range;
    /* names as linked VS_EXPR_IDENT nodes in `names` */
    vs_expr_t *names;
} vs_port_decl_t;

typedef struct vs_net_decl {
    vs_node_t base;
    vs_range_t *range;
    vs_expr_t *names;
} vs_net_decl_t;

typedef struct vs_reg_decl {
    vs_node_t base;
    vs_range_t *range;
    vs_expr_t *names;
} vs_reg_decl_t;

typedef struct vs_cont_assign {
    vs_node_t base;
    vs_expr_t *lhs;
    vs_expr_t *rhs;
} vs_cont_assign_t;

typedef struct vs_always {
    vs_node_t base;
    vs_node_t *event; /* event control or NULL */
    vs_stmt_t *stmt;
} vs_always_t;

typedef struct vs_initial {
    vs_node_t base;
    vs_stmt_t *stmt;
} vs_initial_t;

typedef struct vs_event_control {
    vs_node_t base;
    vs_node_t *expr; /* edge/ident/or list, or NULL meaning @* */
    int star;        /* 1 if @* */
} vs_event_control_t;

typedef struct vs_event_edge {
    vs_node_t base;
    vs_edge_kind_t edge;
    const char *name;
} vs_event_edge_t;

typedef struct vs_event_ident {
    vs_node_t base;
    const char *name;
} vs_event_ident_t;

typedef struct vs_block {
    vs_node_t base;
    vs_stmt_t *stmts;
} vs_block_t;

typedef struct vs_if {
    vs_node_t base;
    vs_expr_t *cond;
    vs_stmt_t *then_stmt;
    vs_stmt_t *else_stmt;
} vs_if_t;

typedef struct vs_assign_stmt {
    vs_node_t base;
    vs_expr_t *lhs;
    vs_expr_t *rhs;
} vs_assign_stmt_t;

typedef struct vs_expr_ident {
    vs_node_t base;
    const char *name;
} vs_expr_ident_t;

typedef struct vs_expr_number {
    vs_node_t base;
    const char *text;
} vs_expr_number_t;

typedef struct vs_expr_unary {
    vs_node_t base;
    vs_unary_op_t op;
    vs_expr_t *expr;
} vs_expr_unary_t;

typedef struct vs_expr_binary {
    vs_node_t base;
    vs_binary_op_t op;
    vs_expr_t *lhs;
    vs_expr_t *rhs;
} vs_expr_binary_t;

typedef struct vs_expr_select {
    vs_node_t base;
    vs_expr_t *expr;
    vs_expr_t *index;
} vs_expr_select_t;

typedef struct vs_expr_part {
    vs_node_t base;
    vs_expr_t *expr;
    vs_expr_t *msb;
    vs_expr_t *lsb;
} vs_expr_part_t;

typedef struct vs_expr_concat {
    vs_node_t base;
    vs_expr_t *exprs;
} vs_expr_concat_t;

vs_design_t *vs_design_new(vs_arena_t *a);
void vs_design_add_module(vs_design_t *d, vs_module_t *m);

vs_module_t *vs_module_new(vs_arena_t *a, vs_loc_t loc, const char *name);
void vs_module_add_port(vs_module_t *m, vs_port_t *p);
void vs_module_add_item(vs_module_t *m, vs_item_t *it);

vs_port_t *vs_port_new(vs_arena_t *a, vs_loc_t loc, vs_port_dir_t dir, vs_range_t *range,
                       const char *name);
vs_range_t *vs_range_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *msb, vs_expr_t *lsb);

vs_port_decl_t *vs_port_decl_new(vs_arena_t *a, vs_loc_t loc, vs_port_dir_t dir, vs_range_t *range,
                                 vs_expr_t *names);
vs_net_decl_t *vs_net_decl_new(vs_arena_t *a, vs_loc_t loc, vs_range_t *range, vs_expr_t *names);
vs_reg_decl_t *vs_reg_decl_new(vs_arena_t *a, vs_loc_t loc, vs_range_t *range, vs_expr_t *names);
vs_cont_assign_t *vs_cont_assign_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *lhs, vs_expr_t *rhs);

vs_always_t *vs_always_new(vs_arena_t *a, vs_loc_t loc, vs_node_t *event, vs_stmt_t *stmt);
vs_initial_t *vs_initial_new(vs_arena_t *a, vs_loc_t loc, vs_stmt_t *stmt);
vs_event_control_t *vs_event_control_new(vs_arena_t *a, vs_loc_t loc, vs_node_t *expr, int star);
vs_event_edge_t *vs_event_edge_new(vs_arena_t *a, vs_loc_t loc, vs_edge_kind_t edge,
                                   const char *name);
vs_event_ident_t *vs_event_ident_new(vs_arena_t *a, vs_loc_t loc, const char *name);
vs_block_t *vs_block_new(vs_arena_t *a, vs_loc_t loc, vs_stmt_t *stmts);
void vs_block_add_stmt(vs_block_t *b, vs_stmt_t *s);
vs_if_t *vs_if_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *cond, vs_stmt_t *then_stmt,
                   vs_stmt_t *else_stmt);
vs_assign_stmt_t *vs_blocking_assign_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *lhs,
                                         vs_expr_t *rhs);
vs_assign_stmt_t *vs_nba_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *lhs, vs_expr_t *rhs);
vs_stmt_t *vs_empty_stmt_new(vs_arena_t *a, vs_loc_t loc);

vs_expr_t *vs_expr_ident_new(vs_arena_t *a, vs_loc_t loc, const char *name);
vs_expr_t *vs_expr_number_new(vs_arena_t *a, vs_loc_t loc, const char *text);
vs_expr_t *vs_expr_unary_new(vs_arena_t *a, vs_loc_t loc, vs_unary_op_t op, vs_expr_t *expr);
vs_expr_t *vs_expr_binary_new(vs_arena_t *a, vs_loc_t loc, vs_binary_op_t op, vs_expr_t *lhs,
                              vs_expr_t *rhs);
vs_expr_t *vs_expr_select_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *expr, vs_expr_t *index);
vs_expr_t *vs_expr_part_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *expr, vs_expr_t *msb,
                            vs_expr_t *lsb);
vs_expr_t *vs_expr_concat_new(vs_arena_t *a, vs_loc_t loc, vs_expr_t *exprs);
void vs_expr_list_append(vs_expr_t **list, vs_expr_t *e);

void vs_node_list_append(vs_node_t **list, vs_node_t *n);

void vs_dump_ast(FILE *out, const vs_design_t *design);

#endif
