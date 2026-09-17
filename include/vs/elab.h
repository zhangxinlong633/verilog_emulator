#ifndef VS_ELAB_H
#define VS_ELAB_H

#include "vs/arena.h"
#include "vs/ast.h"
#include "vs/diag.h"

typedef enum vs_proc_kind {
    VS_PROC_ALWAYS = 1,
    VS_PROC_INITIAL,
    VS_PROC_ASSIGN
} vs_proc_kind_t;

typedef struct vs_signal {
    const char *name;
    int width; /* concrete packed width after parameter const-fold */
    int is_reg;
    int index;
    vs_port_dir_t dir; /* VS_DIR_NONE if not a port */
} vs_signal_t;

typedef struct vs_process {
    int id;
    const char *name;
    vs_proc_kind_t kind;
    int edge_sig; /* signal index, or -1 */
    vs_edge_kind_t edge;
    int level_sensitive; /* 1 for @* / assign */
    vs_stmt_t *stmt;     /* always/initial */
    vs_expr_t *lhs;      /* assign */
    vs_expr_t *rhs;
} vs_process_t;

typedef struct vs_anno_set vs_anno_set_t;

typedef struct vs_netlist {
    const char *module_name;
    vs_signal_t *sigs;
    int nsigs;
    vs_process_t *procs;
    int nprocs;
    const vs_anno_set_t *annos; /* from design; may be NULL */
} vs_netlist_t;

/* Elaborate first module in design (flat). Returns NULL on error. */
vs_netlist_t *vs_elab_flat(vs_arena_t *arena, vs_diag_t *diag, const vs_design_t *design);

int vs_netlist_find_sig(const vs_netlist_t *nl, const char *name);

#endif
