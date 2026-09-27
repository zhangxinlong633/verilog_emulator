#define _POSIX_C_SOURCE 200809L
#include "vs/anno.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *arena_dup(vs_arena_t *a, const char *s) {
    size_t n = strlen(s) + 1;
    char *p = vs_arena_alloc(a, n);
    if (p) {
        memcpy(p, s, n);
    }
    return p;
}

static char *arena_dup_n(vs_arena_t *a, const char *s, size_t n) {
    char *p = vs_arena_alloc(a, n + 1);
    if (p) {
        memcpy(p, s, n);
        p[n] = '\0';
    }
    return p;
}

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

static int starts_with(const char *s, const char *pfx) {
    return strncmp(s, pfx, strlen(pfx)) == 0;
}

static void warn_line(vs_diag_t *diag, const char *path, int lineno, const char *msg) {
    vs_loc_t loc = {.path = path,
                    .first_line = lineno,
                    .first_column = 1,
                    .last_line = lineno,
                    .last_column = 1};
    vs_diag_warn(diag, loc, "%s", msg);
}

static const char *next_token(const char **pp, vs_arena_t *a) {
    const char *p = skip_ws(*pp);
    if (!*p) {
        *pp = p;
        return NULL;
    }
    size_t n = 0;
    while (p[n] && !isspace((unsigned char)p[n])) {
        n++;
    }
    *pp = p + n;
    return arena_dup_n(a, p, n);
}

static int kv_str(vs_arena_t *a, const char *tok, const char *key, const char **out) {
    size_t k = strlen(key);
    if (strncmp(tok, key, k) != 0 || tok[k] != '=') {
        return 0;
    }
    *out = arena_dup(a, tok + k + 1);
    return *out != NULL;
}

static int push_matrix(vs_arena_t *a, vs_anno_set_t *set, vs_anno_matrix_t m) {
    vs_anno_matrix_t *ns = vs_arena_alloc(a, (size_t)(set->nmatrices + 1) * sizeof(*ns));
    if (!ns) {
        return -1;
    }
    if (set->nmatrices > 0) {
        memcpy(ns, set->matrices, (size_t)set->nmatrices * sizeof(*ns));
    }
    ns[set->nmatrices] = m;
    set->matrices = ns;
    set->nmatrices++;
    return 0;
}

static int push_op(vs_arena_t *a, vs_anno_set_t *set, vs_anno_op_t op) {
    vs_anno_op_t *ns = vs_arena_alloc(a, (size_t)(set->nops + 1) * sizeof(*ns));
    if (!ns) {
        return -1;
    }
    if (set->nops > 0) {
        memcpy(ns, set->ops, (size_t)set->nops * sizeof(*ns));
    }
    ns[set->nops] = op;
    set->ops = ns;
    set->nops++;
    return 0;
}

static int push_expr(vs_arena_t *a, vs_anno_set_t *set, vs_anno_expr_t e) {
    vs_anno_expr_t *ns = vs_arena_alloc(a, (size_t)(set->nexprs + 1) * sizeof(*ns));
    if (!ns) {
        return -1;
    }
    if (set->nexprs > 0) {
        memcpy(ns, set->exprs, (size_t)set->nexprs * sizeof(*ns));
    }
    ns[set->nexprs] = e;
    set->exprs = ns;
    set->nexprs++;
    return 0;
}

static int parse_cells(vs_arena_t *a, const char *cells, int rows, int cols, const char ****out) {
    char *buf = arena_dup(a, cells);
    if (!buf) {
        return -1;
    }
    const char ***grid = vs_arena_alloc(a, (size_t)rows * sizeof(*grid));
    if (!grid) {
        return -1;
    }

    int r = 0;
    char *save_row = NULL;
    for (char *row = strtok_r(buf, ";", &save_row); row; row = strtok_r(NULL, ";", &save_row)) {
        if (r >= rows) {
            return -1;
        }
        const char **crow = vs_arena_alloc(a, (size_t)cols * sizeof(*crow));
        if (!crow) {
            return -1;
        }
        int c = 0;
        char *save_col = NULL;
        for (char *tok = strtok_r(row, ",", &save_col); tok; tok = strtok_r(NULL, ",", &save_col)) {
            if (c >= cols) {
                return -1;
            }
            while (*tok && isspace((unsigned char)*tok)) {
                tok++;
            }
            char *end = tok + strlen(tok);
            while (end > tok && isspace((unsigned char)end[-1])) {
                *--end = '\0';
            }
            crow[c++] = arena_dup(a, tok);
        }
        if (c != cols) {
            return -1;
        }
        grid[r++] = crow;
    }
    if (r != rows) {
        return -1;
    }
    *out = grid;
    return 0;
}

static void parse_view_matrix(vs_arena_t *a, vs_diag_t *diag, const char *path, int lineno,
                              const char *rest, vs_anno_set_t *set) {
    const char *p = rest;
    const char *name = next_token(&p, a);
    if (!name) {
        warn_line(diag, path, lineno, "bad @vs view matrix: missing name");
        return;
    }
    int rows = 0, cols = 0;
    const char *cells = NULL;
    const char *array_base = NULL;
    const char *rows_ref = NULL;
    const char *cols_ref = NULL;
    for (;;) {
        const char *tok = next_token(&p, a);
        if (!tok) {
            break;
        }
        if (starts_with(tok, "rows=")) {
            const char *v = tok + 5;
            char *end = NULL;
            long n = strtol(v, &end, 10);
            if (end && *end == '\0' && n > 0) {
                rows = (int)n;
                rows_ref = NULL;
            } else if (*v) {
                rows = 0;
                rows_ref = arena_dup(a, v);
            }
            continue;
        }
        if (starts_with(tok, "cols=")) {
            const char *v = tok + 5;
            char *end = NULL;
            long n = strtol(v, &end, 10);
            if (end && *end == '\0' && n > 0) {
                cols = (int)n;
                cols_ref = NULL;
            } else if (*v) {
                cols = 0;
                cols_ref = arena_dup(a, v);
            }
            continue;
        }
        if (starts_with(tok, "cells=")) {
            cells = arena_dup(a, tok + 6);
            continue;
        }
        if (kv_str(a, tok, "array", &array_base)) {
            continue;
        }
        warn_line(diag, path, lineno, "bad @vs view matrix: unknown field");
        return;
    }
    int have_rows = rows > 0 || rows_ref != NULL;
    int have_cols = cols > 0 || cols_ref != NULL;
    if (!have_rows || !have_cols || (!cells && !array_base)) {
        warn_line(diag, path, lineno, "bad @vs view matrix: incomplete");
        return;
    }
    if (cells && array_base) {
        warn_line(diag, path, lineno, "bad @vs view matrix: cells= and array= both set");
        return;
    }
    const char ***grid = NULL;
    if (cells) {
        if (rows <= 0 || cols <= 0) {
            warn_line(diag, path, lineno, "bad @vs view matrix: cells= needs numeric rows/cols");
            return;
        }
        if (parse_cells(a, cells, rows, cols, &grid) != 0) {
            warn_line(diag, path, lineno, "bad @vs view matrix: cells shape mismatch");
            return;
        }
    }
    vs_anno_matrix_t m = {.name = name,
                          .rows = rows,
                          .cols = cols,
                          .cells = grid,
                          .array_base = array_base,
                          .rows_ref = rows_ref,
                          .cols_ref = cols_ref};
    (void)push_matrix(a, set, m);
}

static int resolve_dim(vs_diag_t *diag, int cur, const char *ref,
                       int (*lookup)(void *ctx, const char *name, int64_t *out), void *ctx,
                       int *out) {
    if (cur > 0) {
        *out = cur;
        return 0;
    }
    if (!ref || !lookup) {
        return -1;
    }
    int64_t v = 0;
    if (lookup(ctx, ref, &v) != 0 || v <= 0 || v > 1024) {
        vs_loc_t loc = {0};
        vs_diag_warn(diag, loc, "bad @vs array dim '%s'", ref);
        return -1;
    }
    *out = (int)v;
    return 0;
}

static const char *elem_name_2d(vs_arena_t *a, const char *base, int i, int j) {
    char buf[160];
    snprintf(buf, sizeof buf, "%s_%d_%d", base, i, j);
    return arena_dup(a, buf);
}

int vs_anno_resolve_params(vs_anno_set_t *set, vs_arena_t *arena, vs_diag_t *diag,
                           int (*lookup)(void *ctx, const char *name, int64_t *out), void *ctx) {
    if (!set || !arena) {
        return -1;
    }
    for (int i = 0; i < set->nmatrices; i++) {
        vs_anno_matrix_t *m = &set->matrices[i];
        if (!m->array_base) {
            continue;
        }
        int rows = 0, cols = 0;
        if (resolve_dim(diag, m->rows, m->rows_ref, lookup, ctx, &rows) != 0 ||
            resolve_dim(diag, m->cols, m->cols_ref, lookup, ctx, &cols) != 0) {
            return -1;
        }
        const char ***grid = vs_arena_alloc(arena, (size_t)rows * sizeof(*grid));
        if (!grid) {
            return -1;
        }
        for (int r = 0; r < rows; r++) {
            const char **crow = vs_arena_alloc(arena, (size_t)cols * sizeof(*crow));
            if (!crow) {
                return -1;
            }
            for (int c = 0; c < cols; c++) {
                crow[c] = elem_name_2d(arena, m->array_base, r, c);
                if (!crow[c]) {
                    return -1;
                }
            }
            grid[r] = crow;
        }
        m->rows = rows;
        m->cols = cols;
        m->cells = grid;
    }
    return 0;
}

static void parse_op(vs_arena_t *a, vs_diag_t *diag, const char *path, int lineno, const char *rest,
                     vs_anno_set_t *set) {
    const char *p = rest;
    const char *kind = next_token(&p, a);
    if (!kind) {
        warn_line(diag, path, lineno, "bad @vs op: missing kind");
        return;
    }
    const char *out = NULL, *left = NULL, *right = NULL;
    for (;;) {
        const char *tok = next_token(&p, a);
        if (!tok) {
            break;
        }
        if (kv_str(a, tok, "out", &out) || kv_str(a, tok, "left", &left) ||
            kv_str(a, tok, "right", &right)) {
            continue;
        }
        warn_line(diag, path, lineno, "bad @vs op: unknown field");
        return;
    }
    if (!out || !left || !right) {
        warn_line(diag, path, lineno, "bad @vs op: incomplete");
        return;
    }
    vs_anno_op_t op = {.kind = kind, .out = out, .left = left, .right = right};
    (void)push_op(a, set, op);
}

static void parse_expr(vs_arena_t *a, vs_diag_t *diag, const char *path, int lineno, const char *rest,
                       vs_anno_set_t *set) {
    const char *p = skip_ws(rest);
    if (!*p) {
        warn_line(diag, path, lineno, "bad @vs expr: missing sig");
        return;
    }
    size_t slen = 0;
    while (p[slen] && !isspace((unsigned char)p[slen]) && p[slen] != '=') {
        slen++;
    }
    const char *sig = arena_dup_n(a, p, slen);
    p = skip_ws(p + slen);
    if (*p != '=') {
        warn_line(diag, path, lineno, "bad @vs expr: missing '='");
        return;
    }
    p = skip_ws(p + 1);
    size_t elen = strlen(p);
    while (elen > 0 && isspace((unsigned char)p[elen - 1])) {
        elen--;
    }
    if (!sig || elen == 0) {
        warn_line(diag, path, lineno, "bad @vs expr: empty rhs");
        return;
    }
    vs_anno_expr_t e = {.sig = sig, .expr = arena_dup_n(a, p, elen)};
    (void)push_expr(a, set, e);
}

static void handle_vs_line(vs_arena_t *a, vs_diag_t *diag, const char *path, int lineno,
                           const char *body, vs_anno_set_t *set) {
    const char *p = skip_ws(body);
    if (starts_with(p, "view")) {
        p = skip_ws(p + 4);
        if (starts_with(p, "matrix")) {
            parse_view_matrix(a, diag, path, lineno, p + 6, set);
        } else {
            warn_line(diag, path, lineno, "bad @vs view: unknown kind");
        }
    } else if (starts_with(p, "op")) {
        parse_op(a, diag, path, lineno, p + 2, set);
    } else if (starts_with(p, "expr")) {
        parse_expr(a, diag, path, lineno, p + 4, set);
    } else {
        warn_line(diag, path, lineno, "unknown @vs verb");
    }
}

vs_anno_set_t *vs_anno_scan_file(vs_arena_t *arena, vs_diag_t *diag, const char *path) {
    vs_anno_set_t *set = vs_arena_alloc(arena, sizeof(*set));
    if (!set) {
        return NULL;
    }
    memset(set, 0, sizeof(*set));
    FILE *fp = fopen(path, "r");
    if (!fp) {
        warn_line(diag, path, 0, "cannot open for @vs scan");
        return set;
    }
    char buf[2048];
    int lineno = 0;
    while (fgets(buf, sizeof buf, fp)) {
        lineno++;
        size_t n = strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
            buf[--n] = '\0';
        }
        const char *p = skip_ws(buf);
        if (!starts_with(p, "//")) {
            continue;
        }
        p = skip_ws(p + 2);
        if (!starts_with(p, "@vs")) {
            continue;
        }
        handle_vs_line(arena, diag, path, lineno, skip_ws(p + 3), set);
    }
    fclose(fp);
    return set;
}

const char *vs_anno_find_expr(const vs_anno_set_t *set, const char *sig) {
    if (!set || !sig) {
        return NULL;
    }
    for (int i = 0; i < set->nexprs; i++) {
        if (strcmp(set->exprs[i].sig, sig) == 0) {
            return set->exprs[i].expr;
        }
    }
    return NULL;
}

static int append_fmt(char *dst, size_t dstsz, size_t *off, const char *fmt, ...) {
    if (*off >= dstsz) {
        return -1;
    }
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(dst + *off, dstsz - *off, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= dstsz - *off) {
        return -1;
    }
    *off += (size_t)n;
    return 0;
}

int vs_anno_append_meta_json(char *dst, size_t dstsz, size_t *off, const vs_anno_set_t *set) {
    if (!set || (set->nmatrices == 0 && set->nops == 0 && set->nexprs == 0)) {
        return 0;
    }
    int nviews = 0;
    for (int i = 0; i < set->nmatrices; i++) {
        if (set->matrices[i].cells && set->matrices[i].rows > 0 && set->matrices[i].cols > 0) {
            nviews++;
        }
    }
    if (nviews > 0) {
        if (append_fmt(dst, dstsz, off, ",\"views\":[") != 0) {
            return -1;
        }
        int emitted = 0;
        for (int i = 0; i < set->nmatrices; i++) {
            const vs_anno_matrix_t *m = &set->matrices[i];
            if (!m->cells || m->rows <= 0 || m->cols <= 0) {
                continue;
            }
            if (emitted > 0 && append_fmt(dst, dstsz, off, ",") != 0) {
                return -1;
            }
            if (append_fmt(dst, dstsz, off,
                           "{\"kind\":\"matrix\",\"name\":\"%s\",\"rows\":%d,\"cols\":%d,\"cells\":[",
                           m->name, m->rows, m->cols) != 0) {
                return -1;
            }
            for (int r = 0; r < m->rows; r++) {
                if (r > 0 && append_fmt(dst, dstsz, off, ",") != 0) {
                    return -1;
                }
                if (append_fmt(dst, dstsz, off, "[") != 0) {
                    return -1;
                }
                for (int c = 0; c < m->cols; c++) {
                    if (c > 0 && append_fmt(dst, dstsz, off, ",") != 0) {
                        return -1;
                    }
                    if (append_fmt(dst, dstsz, off, "\"%s\"", m->cells[r][c]) != 0) {
                        return -1;
                    }
                }
                if (append_fmt(dst, dstsz, off, "]") != 0) {
                    return -1;
                }
            }
            if (append_fmt(dst, dstsz, off, "]}") != 0) {
                return -1;
            }
            emitted++;
        }
        if (append_fmt(dst, dstsz, off, "]") != 0) {
            return -1;
        }
    }
    if (set->nops > 0) {
        if (append_fmt(dst, dstsz, off, ",\"ops\":[") != 0) {
            return -1;
        }
        for (int i = 0; i < set->nops; i++) {
            const vs_anno_op_t *o = &set->ops[i];
            if (i > 0 && append_fmt(dst, dstsz, off, ",") != 0) {
                return -1;
            }
            if (append_fmt(dst, dstsz, off,
                           "{\"kind\":\"%s\",\"out\":\"%s\",\"left\":\"%s\",\"right\":\"%s\"}", o->kind,
                           o->out, o->left, o->right) != 0) {
                return -1;
            }
        }
        if (append_fmt(dst, dstsz, off, "]") != 0) {
            return -1;
        }
    }
    if (set->nexprs > 0) {
        if (append_fmt(dst, dstsz, off, ",\"exprs\":{") != 0) {
            return -1;
        }
        for (int i = 0; i < set->nexprs; i++) {
            if (i > 0 && append_fmt(dst, dstsz, off, ",") != 0) {
                return -1;
            }
            if (append_fmt(dst, dstsz, off, "\"%s\":\"%s\"", set->exprs[i].sig, set->exprs[i].expr) !=
                0) {
                return -1;
            }
        }
        if (append_fmt(dst, dstsz, off, "}") != 0) {
            return -1;
        }
    }
    return 0;
}
