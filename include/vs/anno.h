#ifndef VS_ANNO_H
#define VS_ANNO_H

#include "vs/arena.h"
#include "vs/diag.h"

#include <stddef.h>
#include <stdio.h>

typedef struct vs_anno_matrix {
    const char *name;
    int rows;
    int cols;
    const char ***cells; /* [row][col] signal name; NULL until array= resolve */
    const char *array_base; /* non-NULL when using array= form */
    const char *rows_ref;   /* param name if rows not literal; else NULL */
    const char *cols_ref;   /* param name if cols not literal; else NULL */
} vs_anno_matrix_t;

typedef struct vs_anno_op {
    const char *kind; /* "matmul", ... */
    const char *out;
    const char *left;
    const char *right;
} vs_anno_op_t;

typedef struct vs_anno_expr {
    const char *sig;
    const char *expr; /* rhs string */
} vs_anno_expr_t;

typedef struct vs_anno_set {
    vs_anno_matrix_t *matrices;
    int nmatrices;
    vs_anno_op_t *ops;
    int nops;
    vs_anno_expr_t *exprs;
    int nexprs;
} vs_anno_set_t;

/* Scan path for // @vs lines. Returns set (possibly empty). Never NULL if arena OK. */
vs_anno_set_t *vs_anno_scan_file(vs_arena_t *arena, vs_diag_t *diag, const char *path);

/*
 * Resolve array= / rows=PARAM / cols=PARAM after elab using parameter lookup.
 * Expands cells to base_i_j. No-op for explicit cells= views. Returns 0 on success.
 */
int vs_anno_resolve_params(vs_anno_set_t *set, vs_arena_t *arena, vs_diag_t *diag,
                           int (*lookup)(void *ctx, const char *name, int64_t *out), void *ctx);

/* Lookup expr by signal name; NULL if absent. */
const char *vs_anno_find_expr(const vs_anno_set_t *set, const char *sig);

/*
 * Append meta JSON fragments into dst at *off:
 * ,"views":[...],"ops":[...],"exprs":{...}
 * No-op if set empty. Returns 0 on success, -1 if buffer too small.
 */
int vs_anno_append_meta_json(char *dst, size_t dstsz, size_t *off, const vs_anno_set_t *set);

#endif
