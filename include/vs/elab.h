#ifndef VS_ELAB_H
#define VS_ELAB_H

#include "vs/arena.h"
#include "vs/ast.h"
#include "vs/diag.h"

#include <stdint.h>

typedef enum vs_proc_kind {
    VS_PROC_ALWAYS = 1,
    VS_PROC_INITIAL,
    VS_PROC_ASSIGN
} vs_proc_kind_t;

typedef enum vs_sig_shape {
    VS_SIG_SCALAR = 0,
    VS_SIG_ARRAY1D,
    VS_SIG_ARRAY2D
} vs_sig_shape_t;

typedef struct vs_signal {
    const char *name;
    int width; /* concrete packed width after parameter const-fold */
    int is_reg;
    int index;
    vs_port_dir_t dir; /* VS_DIR_NONE if not a port */
} vs_signal_t;

/* Unpacked array expanded to contiguous scalar signals base_i (or base_i_j). */
typedef struct vs_array {
    const char *base;
    int ndim; /* 1 or 2 */
    int lens[2];
    int lo[2]; /* lower index bound per dim */
    int width;
    int first_sig_index; /* contiguous scalars */
} vs_array_t;

typedef struct vs_netlist_param {
    const char *name;
    int64_t value;
} vs_netlist_param_t;

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
    vs_array_t *arrays;
    int narrays;
    vs_netlist_param_t *params;
    int nparams;
    const char **integers; /* integer variable names */
    int nintegers;
    const vs_anno_set_t *annos; /* from design; may be NULL */
} vs_netlist_t;

/* Elaborate first module in design (flat). Returns NULL on error. */
vs_netlist_t *vs_elab_flat(vs_arena_t *arena, vs_diag_t *diag, const vs_design_t *design);

int vs_netlist_find_sig(const vs_netlist_t *nl, const char *name);
const vs_array_t *vs_netlist_find_array(const vs_netlist_t *nl, const char *base);
int vs_netlist_find_param(const vs_netlist_t *nl, const char *name, int64_t *out);
int vs_netlist_is_integer(const vs_netlist_t *nl, const char *name);

#endif
