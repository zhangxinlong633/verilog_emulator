#include "vs/elab.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    int64_t value;
} vs_param_binding_t;

typedef struct {
    vs_param_binding_t *items;
    int n;
    int cap;
} vs_param_env_t;

static int param_env_lookup(const vs_param_env_t *env, const char *name, int64_t *out) {
    if (!env || !name || !out) {
        return -1;
    }
    for (int i = 0; i < env->n; i++) {
        if (strcmp(env->items[i].name, name) == 0) {
            *out = env->items[i].value;
            return 0;
        }
    }
    return -1;
}

static int param_env_add(vs_arena_t *a, vs_param_env_t *env, const char *name, int64_t value) {
    if (!a || !env || !name) {
        return -1;
    }
    if (env->n >= env->cap) {
        int ncap = env->cap ? env->cap * 2 : 8;
        vs_param_binding_t *ni = vs_arena_alloc(a, (size_t)ncap * sizeof(vs_param_binding_t));
        if (env->n > 0) {
            memcpy(ni, env->items, (size_t)env->n * sizeof(vs_param_binding_t));
        }
        env->items = ni;
        env->cap = ncap;
    }
    env->items[env->n].name = name;
    env->items[env->n].value = value;
    env->n++;
    return 0;
}

static int parse_number_text(const char *s, int64_t *out) {
    if (!s || !out) {
        return -1;
    }
    /* Support plain decimal or sized like 4'd0 / 4'hA */
    const char *p = strchr(s, '\'');
    if (!p) {
        *out = (int64_t)strtoll(s, NULL, 10);
        return 0;
    }
    char base = p[1];
    const char *digits = p + 2;
    if (base == 's' || base == 'S') {
        base = p[2];
        digits = p + 3;
    }
    int b = 10;
    if (base == 'h' || base == 'H') {
        b = 16;
    } else if (base == 'b' || base == 'B') {
        b = 2;
    } else if (base == 'o' || base == 'O') {
        b = 8;
    } else if (base == 'd' || base == 'D') {
        b = 10;
    }
    *out = (int64_t)strtoll(digits, NULL, b);
    return 0;
}

/* Fold const expr using parameter env. Returns 0 on success. */
static int eval_const_expr(vs_diag_t *diag, const vs_param_env_t *env, const vs_expr_t *e,
                           int64_t *out) {
    if (!e || !out) {
        return -1;
    }
    switch (e->base.kind) {
    case VS_EXPR_NUMBER: {
        const vs_expr_number_t *n = (const vs_expr_number_t *)e;
        if (parse_number_text(n->text, out)) {
            vs_diag_error(diag, e->base.loc, "invalid number literal in constant expression");
            return -1;
        }
        return 0;
    }
    case VS_EXPR_IDENT: {
        const vs_expr_ident_t *id = (const vs_expr_ident_t *)e;
        if (param_env_lookup(env, id->name, out)) {
            vs_diag_error(diag, e->base.loc,
                          "identifier '%s' is not a parameter in constant expression", id->name);
            return -1;
        }
        return 0;
    }
    case VS_EXPR_UNARY: {
        const vs_expr_unary_t *u = (const vs_expr_unary_t *)e;
        int64_t v = 0;
        if (eval_const_expr(diag, env, u->expr, &v)) {
            return -1;
        }
        switch (u->op) {
        case VS_UOP_PLUS:
            *out = v;
            return 0;
        case VS_UOP_MINUS:
            *out = -v;
            return 0;
        default:
            vs_diag_error(diag, e->base.loc, "unary operator not allowed in constant expression");
            return -1;
        }
    }
    case VS_EXPR_BINARY: {
        const vs_expr_binary_t *b = (const vs_expr_binary_t *)e;
        int64_t lhs = 0, rhs = 0;
        if (eval_const_expr(diag, env, b->lhs, &lhs) || eval_const_expr(diag, env, b->rhs, &rhs)) {
            return -1;
        }
        switch (b->op) {
        case VS_BOP_ADD:
            *out = lhs + rhs;
            return 0;
        case VS_BOP_SUB:
            *out = lhs - rhs;
            return 0;
        case VS_BOP_MUL:
            *out = lhs * rhs;
            return 0;
        case VS_BOP_DIV:
            if (rhs == 0) {
                vs_diag_error(diag, e->base.loc, "division by zero in constant expression");
                return -1;
            }
            *out = lhs / rhs;
            return 0;
        default:
            vs_diag_error(diag, e->base.loc, "binary operator not allowed in constant expression");
            return -1;
        }
    }
    default:
        vs_diag_error(diag, e->base.loc, "non-constant expression in packed range");
        return -1;
    }
}

static int range_bounds(vs_diag_t *diag, const vs_param_env_t *env, const vs_range_t *r, int *lo,
                        int *hi) {
    if (!r || !lo || !hi) {
        return -1;
    }
    int64_t msb = 0, lsb = 0;
    if (eval_const_expr(diag, env, r->msb, &msb) || eval_const_expr(diag, env, r->lsb, &lsb)) {
        return -1;
    }
    *lo = (int)(msb < lsb ? msb : lsb);
    *hi = (int)(msb > lsb ? msb : lsb);
    return 0;
}

static int range_width(vs_diag_t *diag, const vs_param_env_t *env, const vs_range_t *r, int *out) {
    if (!out) {
        return -1;
    }
    if (!r) {
        *out = 1;
        return 0;
    }
    int lo = 0, hi = 0;
    if (range_bounds(diag, env, r, &lo, &hi)) {
        return -1;
    }
    *out = hi - lo + 1;
    if (*out <= 0) {
        vs_diag_error(diag, r->base.loc, "invalid packed range width");
        return -1;
    }
    return 0;
}

static const char *arena_elem_name(vs_arena_t *a, const char *base, int idx) {
    char buf[128];
    snprintf(buf, sizeof buf, "%s_%d", base, idx);
    size_t n = strlen(buf) + 1;
    char *s = vs_arena_alloc(a, n);
    memcpy(s, buf, n);
    return s;
}

static const char *arena_elem_name2d(vs_arena_t *a, const char *base, int i, int j) {
    char buf[160];
    snprintf(buf, sizeof buf, "%s_%d_%d", base, i, j);
    size_t n = strlen(buf) + 1;
    char *s = vs_arena_alloc(a, n);
    memcpy(s, buf, n);
    return s;
}

static vs_signal_t *add_sig(vs_arena_t *a, vs_netlist_t *nl, const char *name, int width, int is_reg,
                            vs_port_dir_t dir) {
    for (int i = 0; i < nl->nsigs; i++) {
        if (strcmp(nl->sigs[i].name, name) == 0) {
            if (width > nl->sigs[i].width) {
                nl->sigs[i].width = width;
            }
            if (is_reg) {
                nl->sigs[i].is_reg = 1;
            }
            if (dir != VS_DIR_NONE) {
                nl->sigs[i].dir = dir;
            }
            return &nl->sigs[i];
        }
    }
    /* grow */
    int ncap = nl->nsigs + 1;
    vs_signal_t *ns = vs_arena_alloc(a, (size_t)ncap * sizeof(vs_signal_t));
    if (nl->nsigs > 0) {
        memcpy(ns, nl->sigs, (size_t)nl->nsigs * sizeof(vs_signal_t));
    }
    nl->sigs = ns;
    vs_signal_t *s = &nl->sigs[nl->nsigs];
    s->name = name;
    s->width = width > 0 ? width : 1;
    s->is_reg = is_reg;
    s->index = nl->nsigs;
    s->dir = dir;
    nl->nsigs++;
    return s;
}

static int add_array1d(vs_arena_t *a, vs_diag_t *diag, vs_param_env_t *env, vs_netlist_t *nl,
                       const char *base, vs_range_t *packed, vs_range_t *unpacked, int is_reg,
                       vs_port_dir_t dir) {
    int width = 0;
    if (range_width(diag, env, packed, &width)) {
        return -1;
    }
    int lo = 0, hi = 0;
    if (range_bounds(diag, env, unpacked, &lo, &hi)) {
        return -1;
    }
    int len = hi - lo + 1;
    if (len <= 0) {
        vs_diag_error(diag, unpacked->base.loc, "invalid unpacked array length");
        return -1;
    }

    int first = nl->nsigs;
    for (int i = lo; i <= hi; i++) {
        const char *ename = arena_elem_name(a, base, i);
        add_sig(a, nl, ename, width, is_reg, dir);
    }

    int ncap = nl->narrays + 1;
    vs_array_t *na = vs_arena_alloc(a, (size_t)ncap * sizeof(vs_array_t));
    if (nl->narrays > 0) {
        memcpy(na, nl->arrays, (size_t)nl->narrays * sizeof(vs_array_t));
    }
    nl->arrays = na;
    vs_array_t *arr = &nl->arrays[nl->narrays++];
    arr->base = base;
    arr->ndim = 1;
    arr->lens[0] = len;
    arr->lens[1] = 0;
    arr->lo[0] = lo;
    arr->lo[1] = 0;
    arr->width = width;
    arr->first_sig_index = first;
    return 0;
}

static int add_array2d(vs_arena_t *a, vs_diag_t *diag, vs_param_env_t *env, vs_netlist_t *nl,
                       const char *base, vs_range_t *packed, vs_range_t *dim0, vs_range_t *dim1,
                       int is_reg, vs_port_dir_t dir) {
    int width = 0;
    if (range_width(diag, env, packed, &width)) {
        return -1;
    }
    int lo0 = 0, hi0 = 0, lo1 = 0, hi1 = 0;
    if (range_bounds(diag, env, dim0, &lo0, &hi0) || range_bounds(diag, env, dim1, &lo1, &hi1)) {
        return -1;
    }
    int rows = hi0 - lo0 + 1;
    int cols = hi1 - lo1 + 1;
    if (rows <= 0 || cols <= 0) {
        vs_diag_error(diag, dim0->base.loc, "invalid 2D unpacked array length");
        return -1;
    }

    int first = nl->nsigs;
    for (int i = lo0; i <= hi0; i++) {
        for (int j = lo1; j <= hi1; j++) {
            const char *ename = arena_elem_name2d(a, base, i, j);
            add_sig(a, nl, ename, width, is_reg, dir);
        }
    }

    int ncap = nl->narrays + 1;
    vs_array_t *na = vs_arena_alloc(a, (size_t)ncap * sizeof(vs_array_t));
    if (nl->narrays > 0) {
        memcpy(na, nl->arrays, (size_t)nl->narrays * sizeof(vs_array_t));
    }
    nl->arrays = na;
    vs_array_t *arr = &nl->arrays[nl->narrays++];
    arr->base = base;
    arr->ndim = 2;
    arr->lens[0] = rows;
    arr->lens[1] = cols;
    arr->lo[0] = lo0;
    arr->lo[1] = lo1;
    arr->width = width;
    arr->first_sig_index = first;
    return 0;
}

static int add_unpacked_array(vs_arena_t *a, vs_diag_t *diag, vs_param_env_t *env, vs_netlist_t *nl,
                              const char *base, vs_range_t *packed, vs_range_t *unpacked,
                              int is_reg, vs_port_dir_t dir) {
    if (!unpacked) {
        return -1;
    }
    vs_range_t *dim1 = (vs_range_t *)unpacked->base.next;
    if (!dim1) {
        return add_array1d(a, diag, env, nl, base, packed, unpacked, is_reg, dir);
    }
    if (dim1->base.next) {
        vs_diag_error(diag, unpacked->base.loc, "arrays deeper than 2D are not supported");
        return -1;
    }
    return add_array2d(a, diag, env, nl, base, packed, unpacked, dim1, is_reg, dir);
}

static int add_names(vs_arena_t *a, vs_diag_t *diag, vs_param_env_t *env, vs_netlist_t *nl,
                     vs_expr_t *names, vs_range_t *range, vs_range_t *unpacked, int is_reg,
                     vs_port_dir_t dir) {
    if (unpacked) {
        for (vs_expr_t *e = names; e; e = (vs_expr_t *)e->base.next) {
            if (e->base.kind != VS_EXPR_IDENT) {
                continue;
            }
            const vs_expr_ident_t *id = (const vs_expr_ident_t *)e;
            if (add_unpacked_array(a, diag, env, nl, id->name, range, unpacked, is_reg, dir)) {
                return -1;
            }
        }
        return 0;
    }
    int w = 0;
    if (range_width(diag, env, range, &w)) {
        return -1;
    }
    for (vs_expr_t *e = names; e; e = (vs_expr_t *)e->base.next) {
        if (e->base.kind != VS_EXPR_IDENT) {
            continue;
        }
        const vs_expr_ident_t *id = (const vs_expr_ident_t *)e;
        add_sig(a, nl, id->name, w, is_reg, dir);
    }
    return 0;
}

static char *proc_name(vs_arena_t *a, const char *mod, const char *kind, int id) {
    char buf[128];
    snprintf(buf, sizeof buf, "%s.%s%d", mod ? mod : "m", kind, id);
    size_t n = strlen(buf) + 1;
    char *s = vs_arena_alloc(a, n);
    memcpy(s, buf, n);
    return s;
}

static void add_proc(vs_arena_t *a, vs_netlist_t *nl, vs_process_t p) {
    int ncap = nl->nprocs + 1;
    vs_process_t *np = vs_arena_alloc(a, (size_t)ncap * sizeof(vs_process_t));
    if (nl->nprocs > 0) {
        memcpy(np, nl->procs, (size_t)nl->nprocs * sizeof(vs_process_t));
    }
    nl->procs = np;
    p.id = nl->nprocs;
    nl->procs[nl->nprocs++] = p;
}

int vs_netlist_find_sig(const vs_netlist_t *nl, const char *name) {
    if (!nl || !name) {
        return -1;
    }
    for (int i = 0; i < nl->nsigs; i++) {
        if (strcmp(nl->sigs[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

const vs_array_t *vs_netlist_find_array(const vs_netlist_t *nl, const char *base) {
    if (!nl || !base) {
        return NULL;
    }
    for (int i = 0; i < nl->narrays; i++) {
        if (strcmp(nl->arrays[i].base, base) == 0) {
            return &nl->arrays[i];
        }
    }
    return NULL;
}

int vs_netlist_find_param(const vs_netlist_t *nl, const char *name, int64_t *out) {
    if (!nl || !name || !out) {
        return -1;
    }
    for (int i = 0; i < nl->nparams; i++) {
        if (strcmp(nl->params[i].name, name) == 0) {
            *out = nl->params[i].value;
            return 0;
        }
    }
    return -1;
}

int vs_netlist_is_integer(const vs_netlist_t *nl, const char *name) {
    if (!nl || !name) {
        return 0;
    }
    for (int i = 0; i < nl->nintegers; i++) {
        if (strcmp(nl->integers[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int build_param_env(vs_arena_t *arena, vs_diag_t *diag, const vs_module_t *m,
                           vs_param_env_t *env) {
    memset(env, 0, sizeof(*env));
    for (const vs_param_decl_t *p = m->params; p; p = (const vs_param_decl_t *)p->base.next) {
        int64_t v = 0;
        if (eval_const_expr(diag, env, p->value, &v)) {
            return -1;
        }
        if (param_env_add(arena, env, p->name, v)) {
            return -1;
        }
    }
    return 0;
}

static void copy_params_to_netlist(vs_arena_t *arena, vs_netlist_t *nl, const vs_param_env_t *env) {
    if (!env || env->n <= 0) {
        return;
    }
    nl->params = vs_arena_alloc(arena, (size_t)env->n * sizeof(vs_netlist_param_t));
    nl->nparams = env->n;
    for (int i = 0; i < env->n; i++) {
        nl->params[i].name = env->items[i].name;
        nl->params[i].value = env->items[i].value;
    }
}

static int add_integer_names(vs_arena_t *a, vs_netlist_t *nl, vs_expr_t *names) {
    for (vs_expr_t *e = names; e; e = (vs_expr_t *)e->base.next) {
        if (e->base.kind != VS_EXPR_IDENT) {
            continue;
        }
        const vs_expr_ident_t *id = (const vs_expr_ident_t *)e;
        int ncap = nl->nintegers + 1;
        const char **ni = vs_arena_alloc(a, (size_t)ncap * sizeof(const char *));
        if (nl->nintegers > 0) {
            memcpy(ni, nl->integers, (size_t)nl->nintegers * sizeof(const char *));
        }
        nl->integers = ni;
        nl->integers[nl->nintegers++] = id->name;
    }
    return 0;
}

typedef struct {
    const char *from;
    const char *to;
} vs_alias_t;

typedef struct {
    vs_alias_t *items;
    int n;
    int cap;
} vs_alias_map_t;

static int alias_map_add(vs_arena_t *a, vs_alias_map_t *map, const char *from, const char *to) {
    if (!a || !map || !from || !to) {
        return -1;
    }
    if (map->n >= map->cap) {
        int ncap = map->cap ? map->cap * 2 : 8;
        vs_alias_t *ni = vs_arena_alloc(a, (size_t)ncap * sizeof(vs_alias_t));
        if (map->n > 0) {
            memcpy(ni, map->items, (size_t)map->n * sizeof(vs_alias_t));
        }
        map->items = ni;
        map->cap = ncap;
    }
    map->items[map->n].from = from;
    map->items[map->n].to = to;
    map->n++;
    return 0;
}

static const char *alias_map_lookup(const vs_alias_map_t *map, const char *from) {
    if (!map || !from) {
        return NULL;
    }
    for (int i = 0; i < map->n; i++) {
        if (strcmp(map->items[i].from, from) == 0) {
            return map->items[i].to;
        }
    }
    return NULL;
}

static const char *arena_strdup_name(vs_arena_t *a, const char *s) {
    size_t n = strlen(s) + 1;
    char *o = vs_arena_alloc(a, n);
    memcpy(o, s, n);
    return o;
}

static const char *prefixed_name(vs_arena_t *a, const char *prefix, const char *name) {
    if (!prefix || !prefix[0]) {
        return name;
    }
    char buf[256];
    snprintf(buf, sizeof buf, "%s_%s", prefix, name);
    return arena_strdup_name(a, buf);
}

static const char *arena_i64_text(vs_arena_t *a, int64_t v) {
    char buf[32];
    snprintf(buf, sizeof buf, "%lld", (long long)v);
    return arena_strdup_name(a, buf);
}

static const vs_module_t *find_module(const vs_design_t *design, const char *name) {
    if (!design || !name) {
        return NULL;
    }
    for (const vs_module_t *m = design->modules; m; m = (const vs_module_t *)m->base.next) {
        if (m->name && strcmp(m->name, name) == 0) {
            return m;
        }
    }
    return NULL;
}

static int build_param_env_overrides(vs_arena_t *arena, vs_diag_t *diag, const vs_module_t *m,
                                     const vs_param_env_t *parent_env, const vs_named_conn_t *overrides,
                                     vs_param_env_t *env) {
    if (build_param_env(arena, diag, m, env)) {
        return -1;
    }
    for (const vs_named_conn_t *c = overrides; c; c = (const vs_named_conn_t *)c->base.next) {
        int64_t v = 0;
        if (eval_const_expr(diag, parent_env, c->expr, &v)) {
            return -1;
        }
        int found = 0;
        for (int i = 0; i < env->n; i++) {
            if (strcmp(env->items[i].name, c->name) == 0) {
                env->items[i].value = v;
                found = 1;
                break;
            }
        }
        if (!found) {
            if (param_env_add(arena, env, c->name, v)) {
                return -1;
            }
        }
    }
    return 0;
}

static vs_expr_t *rewrite_expr(vs_arena_t *a, const vs_expr_t *e, const char *prefix,
                               const vs_alias_map_t *aliases, const vs_param_env_t *env);
static vs_stmt_t *rewrite_stmt(vs_arena_t *a, const vs_stmt_t *s, const char *prefix,
                               const vs_alias_map_t *aliases, const vs_param_env_t *env);
static vs_node_t *rewrite_event(vs_arena_t *a, const vs_node_t *n, const char *prefix,
                                const vs_alias_map_t *aliases, const vs_param_env_t *env);

static const char *map_local_name(vs_arena_t *a, const char *name, const char *prefix,
                                  const vs_alias_map_t *aliases, const vs_param_env_t *env,
                                  int *is_param, int64_t *param_val) {
    if (is_param) {
        *is_param = 0;
    }
    if (!name) {
        return NULL;
    }
    const char *aliased = alias_map_lookup(aliases, name);
    if (aliased) {
        return aliased;
    }
    if (env && param_val && param_env_lookup(env, name, param_val) == 0) {
        if (is_param) {
            *is_param = 1;
        }
        return NULL;
    }
    return prefixed_name(a, prefix, name);
}

static vs_expr_t *rewrite_expr_list(vs_arena_t *a, const vs_expr_t *list, const char *prefix,
                                    const vs_alias_map_t *aliases, const vs_param_env_t *env) {
    vs_expr_t *out = NULL;
    for (const vs_expr_t *e = list; e; e = (const vs_expr_t *)e->base.next) {
        vs_expr_t *c = rewrite_expr(a, e, prefix, aliases, env);
        if (!c) {
            return NULL;
        }
        vs_expr_list_append(&out, c);
    }
    return out;
}

static vs_expr_t *rewrite_expr(vs_arena_t *a, const vs_expr_t *e, const char *prefix,
                               const vs_alias_map_t *aliases, const vs_param_env_t *env) {
    if (!e) {
        return NULL;
    }
    switch (e->base.kind) {
    case VS_EXPR_IDENT: {
        const vs_expr_ident_t *id = (const vs_expr_ident_t *)e;
        int is_param = 0;
        int64_t pv = 0;
        const char *n = map_local_name(a, id->name, prefix, aliases, env, &is_param, &pv);
        if (is_param) {
            return vs_expr_number_new(a, e->base.loc, arena_i64_text(a, pv));
        }
        return vs_expr_ident_new(a, e->base.loc, n);
    }
    case VS_EXPR_NUMBER: {
        const vs_expr_number_t *n = (const vs_expr_number_t *)e;
        return vs_expr_number_new(a, e->base.loc, n->text);
    }
    case VS_EXPR_UNARY: {
        const vs_expr_unary_t *u = (const vs_expr_unary_t *)e;
        return vs_expr_unary_new(a, e->base.loc, u->op, rewrite_expr(a, u->expr, prefix, aliases, env));
    }
    case VS_EXPR_BINARY: {
        const vs_expr_binary_t *b = (const vs_expr_binary_t *)e;
        return vs_expr_binary_new(a, e->base.loc, b->op, rewrite_expr(a, b->lhs, prefix, aliases, env),
                                  rewrite_expr(a, b->rhs, prefix, aliases, env));
    }
    case VS_EXPR_SELECT: {
        const vs_expr_select_t *s = (const vs_expr_select_t *)e;
        return vs_expr_select_new(a, e->base.loc, rewrite_expr(a, s->expr, prefix, aliases, env),
                                  rewrite_expr(a, s->index, prefix, aliases, env));
    }
    case VS_EXPR_INDEX: {
        const vs_expr_index_t *s = (const vs_expr_index_t *)e;
        return vs_expr_index_new(a, e->base.loc, rewrite_expr(a, s->expr, prefix, aliases, env),
                                 rewrite_expr(a, s->index, prefix, aliases, env));
    }
    case VS_EXPR_PART: {
        const vs_expr_part_t *p = (const vs_expr_part_t *)e;
        return vs_expr_part_new(a, e->base.loc, rewrite_expr(a, p->expr, prefix, aliases, env),
                                rewrite_expr(a, p->msb, prefix, aliases, env),
                                rewrite_expr(a, p->lsb, prefix, aliases, env));
    }
    case VS_EXPR_CONCAT: {
        const vs_expr_concat_t *c = (const vs_expr_concat_t *)e;
        return vs_expr_concat_new(a, e->base.loc, rewrite_expr_list(a, c->exprs, prefix, aliases, env));
    }
    default:
        return NULL;
    }
}

static vs_stmt_t *rewrite_stmt_list(vs_arena_t *a, const vs_stmt_t *list, const char *prefix,
                                    const vs_alias_map_t *aliases, const vs_param_env_t *env) {
    vs_stmt_t *out = NULL;
    for (const vs_stmt_t *s = list; s; s = (const vs_stmt_t *)s->base.next) {
        vs_stmt_t *c = rewrite_stmt(a, s, prefix, aliases, env);
        if (!c) {
            return NULL;
        }
        vs_node_list_append((vs_node_t **)&out, (vs_node_t *)c);
    }
    return out;
}

static vs_stmt_t *rewrite_stmt(vs_arena_t *a, const vs_stmt_t *s, const char *prefix,
                               const vs_alias_map_t *aliases, const vs_param_env_t *env) {
    if (!s) {
        return NULL;
    }
    switch (s->base.kind) {
    case VS_BLOCK: {
        const vs_block_t *b = (const vs_block_t *)s;
        return (vs_stmt_t *)vs_block_new(a, s->base.loc,
                                         rewrite_stmt_list(a, b->stmts, prefix, aliases, env));
    }
    case VS_IF: {
        const vs_if_t *i = (const vs_if_t *)s;
        return (vs_stmt_t *)vs_if_new(a, s->base.loc, rewrite_expr(a, i->cond, prefix, aliases, env),
                                      rewrite_stmt(a, i->then_stmt, prefix, aliases, env),
                                      rewrite_stmt(a, i->else_stmt, prefix, aliases, env));
    }
    case VS_FOR: {
        const vs_for_t *f = (const vs_for_t *)s;
        return (vs_stmt_t *)vs_for_new(a, s->base.loc, rewrite_stmt(a, f->init, prefix, aliases, env),
                                       rewrite_expr(a, f->cond, prefix, aliases, env),
                                       rewrite_stmt(a, f->step, prefix, aliases, env),
                                       rewrite_stmt(a, f->body, prefix, aliases, env));
    }
    case VS_BLOCKING_ASSIGN: {
        const vs_assign_stmt_t *as = (const vs_assign_stmt_t *)s;
        return (vs_stmt_t *)vs_blocking_assign_new(a, s->base.loc,
                                                   rewrite_expr(a, as->lhs, prefix, aliases, env),
                                                   rewrite_expr(a, as->rhs, prefix, aliases, env));
    }
    case VS_NBA: {
        const vs_assign_stmt_t *as = (const vs_assign_stmt_t *)s;
        return (vs_stmt_t *)vs_nba_new(a, s->base.loc, rewrite_expr(a, as->lhs, prefix, aliases, env),
                                       rewrite_expr(a, as->rhs, prefix, aliases, env));
    }
    case VS_EMPTY_STMT:
        return vs_empty_stmt_new(a, s->base.loc);
    case VS_EVENT_CONTROL: {
        /* rare as stmt; clone via rewrite_event */
        return (vs_stmt_t *)rewrite_event(a, (const vs_node_t *)s, prefix, aliases, env);
    }
    default:
        return NULL;
    }
}

static vs_node_t *rewrite_event(vs_arena_t *a, const vs_node_t *n, const char *prefix,
                                const vs_alias_map_t *aliases, const vs_param_env_t *env) {
    if (!n) {
        return NULL;
    }
    if (n->kind == VS_EVENT_CONTROL) {
        const vs_event_control_t *ec = (const vs_event_control_t *)n;
        if (ec->star) {
            return (vs_node_t *)vs_event_control_new(a, n->loc, NULL, 1);
        }
        vs_node_t *olist = NULL;
        for (const vs_node_t *e = ec->expr; e; e = e->next) {
            vs_node_t *c = rewrite_event(a, e, prefix, aliases, env);
            if (!c) {
                return NULL;
            }
            vs_node_list_append(&olist, c);
        }
        return (vs_node_t *)vs_event_control_new(a, n->loc, olist, 0);
    }
    if (n->kind == VS_EVENT_EDGE) {
        const vs_event_edge_t *ee = (const vs_event_edge_t *)n;
        int is_param = 0;
        int64_t pv = 0;
        const char *nm = map_local_name(a, ee->name, prefix, aliases, env, &is_param, &pv);
        if (is_param) {
            nm = ee->name;
        }
        return (vs_node_t *)vs_event_edge_new(a, n->loc, ee->edge, nm);
    }
    if (n->kind == VS_EVENT_IDENT) {
        const vs_event_ident_t *ei = (const vs_event_ident_t *)n;
        int is_param = 0;
        int64_t pv = 0;
        const char *nm = map_local_name(a, ei->name, prefix, aliases, env, &is_param, &pv);
        if (is_param) {
            nm = ei->name;
        }
        return (vs_node_t *)vs_event_ident_new(a, n->loc, nm);
    }
    return NULL;
}

static int add_integer_names_prefixed(vs_arena_t *a, vs_netlist_t *nl, vs_expr_t *names,
                                      const char *prefix) {
    for (vs_expr_t *e = names; e; e = (vs_expr_t *)e->base.next) {
        if (e->base.kind != VS_EXPR_IDENT) {
            continue;
        }
        const vs_expr_ident_t *id = (const vs_expr_ident_t *)e;
        const char *nm = prefixed_name(a, prefix, id->name);
        int ncap = nl->nintegers + 1;
        const char **ni = vs_arena_alloc(a, (size_t)ncap * sizeof(const char *));
        if (nl->nintegers > 0) {
            memcpy(ni, nl->integers, (size_t)nl->nintegers * sizeof(const char *));
        }
        nl->integers = ni;
        nl->integers[nl->nintegers++] = nm;
    }
    return 0;
}

static int add_names_prefixed(vs_arena_t *a, vs_diag_t *diag, vs_param_env_t *env, vs_netlist_t *nl,
                              vs_expr_t *names, vs_range_t *range, vs_range_t *unpacked, int is_reg,
                              vs_port_dir_t dir, const char *prefix) {
    if (unpacked) {
        for (vs_expr_t *e = names; e; e = (vs_expr_t *)e->base.next) {
            if (e->base.kind != VS_EXPR_IDENT) {
                continue;
            }
            const vs_expr_ident_t *id = (const vs_expr_ident_t *)e;
            const char *base = prefixed_name(a, prefix, id->name);
            if (add_unpacked_array(a, diag, env, nl, base, range, unpacked, is_reg, dir)) {
                return -1;
            }
        }
        return 0;
    }
    int w = 0;
    if (range_width(diag, env, range, &w)) {
        return -1;
    }
    for (vs_expr_t *e = names; e; e = (vs_expr_t *)e->base.next) {
        if (e->base.kind != VS_EXPR_IDENT) {
            continue;
        }
        const vs_expr_ident_t *id = (const vs_expr_ident_t *)e;
        add_sig(a, nl, prefixed_name(a, prefix, id->name), w, is_reg, dir);
    }
    return 0;
}

static int elab_module_into(vs_arena_t *arena, vs_diag_t *diag, const vs_design_t *design,
                            const vs_module_t *m, vs_netlist_t *nl, const char *prefix,
                            vs_param_env_t *env, const vs_alias_map_t *port_aliases);

static int elab_instance(vs_arena_t *arena, vs_diag_t *diag, const vs_design_t *design,
                         vs_netlist_t *nl, const vs_instance_t *inst, const char *parent_prefix,
                         const vs_param_env_t *parent_env) {
    const vs_module_t *child = find_module(design, inst->mod_name);
    if (!child) {
        vs_diag_error(diag, inst->base.loc, "unknown module '%s'", inst->mod_name);
        return -1;
    }

    vs_param_env_t child_env;
    if (build_param_env_overrides(arena, diag, child, parent_env, inst->params, &child_env)) {
        return -1;
    }

    vs_alias_map_t aliases;
    memset(&aliases, 0, sizeof(aliases));
    for (const vs_named_conn_t *c = inst->ports; c; c = (const vs_named_conn_t *)c->base.next) {
        if (!c->expr || c->expr->base.kind != VS_EXPR_IDENT) {
            vs_diag_error(diag, c->base.loc,
                          "instance port '.%s' must connect to a simple identifier (array/scalar)",
                          c->name ? c->name : "?");
            return -1;
        }
        const vs_expr_ident_t *id = (const vs_expr_ident_t *)c->expr;
        if (alias_map_add(arena, &aliases, c->name, id->name)) {
            return -1;
        }
    }
    for (const vs_port_t *p = child->ports; p; p = (const vs_port_t *)p->base.next) {
        if (!alias_map_lookup(&aliases, p->name)) {
            vs_diag_error(diag, inst->base.loc, "instance '%s': port '%s' is not connected",
                          inst->inst_name ? inst->inst_name : "?", p->name ? p->name : "?");
            return -1;
        }
    }

    const char *child_prefix = prefixed_name(arena, parent_prefix, inst->inst_name);
    return elab_module_into(arena, diag, design, child, nl, child_prefix, &child_env, &aliases);
}

static int elab_module_into(vs_arena_t *arena, vs_diag_t *diag, const vs_design_t *design,
                            const vs_module_t *m, vs_netlist_t *nl, const char *prefix,
                            vs_param_env_t *env, const vs_alias_map_t *port_aliases) {
    const int is_top = (port_aliases == NULL);

    if (is_top) {
        for (const vs_port_t *p = m->ports; p; p = (const vs_port_t *)p->base.next) {
            int is_reg = (p->dir == VS_DIR_OUTPUT);
            if (p->unpacked_dims) {
                if (add_unpacked_array(arena, diag, env, nl, p->name, p->range, p->unpacked_dims,
                                       is_reg, p->dir)) {
                    return -1;
                }
                continue;
            }
            int w = 0;
            if (range_width(diag, env, p->range, &w)) {
                return -1;
            }
            add_sig(arena, nl, p->name, w, is_reg, p->dir);
        }
    }

    for (const vs_item_t *it = m->items; it; it = (const vs_item_t *)it->base.next) {
        switch (it->base.kind) {
        case VS_PORT_DECL: {
            const vs_port_decl_t *d = (const vs_port_decl_t *)it;
            if (add_names_prefixed(arena, diag, env, nl, d->names, d->range, NULL, 0, d->dir,
                                   prefix)) {
                return -1;
            }
            break;
        }
        case VS_NET_DECL: {
            const vs_net_decl_t *d = (const vs_net_decl_t *)it;
            if (add_names_prefixed(arena, diag, env, nl, d->names, d->range, d->unpacked_dims, 0,
                                   VS_DIR_NONE, prefix)) {
                return -1;
            }
            break;
        }
        case VS_REG_DECL: {
            const vs_reg_decl_t *d = (const vs_reg_decl_t *)it;
            if (add_names_prefixed(arena, diag, env, nl, d->names, d->range, d->unpacked_dims, 1,
                                   VS_DIR_NONE, prefix)) {
                return -1;
            }
            break;
        }
        case VS_INTEGER_DECL: {
            const vs_integer_decl_t *d = (const vs_integer_decl_t *)it;
            if (add_integer_names_prefixed(arena, nl, d->names, prefix)) {
                return -1;
            }
            break;
        }
        case VS_CONT_ASSIGN: {
            const vs_cont_assign_t *c = (const vs_cont_assign_t *)it;
            vs_process_t pr = {0};
            pr.kind = VS_PROC_ASSIGN;
            pr.level_sensitive = 1;
            pr.edge_sig = -1;
            pr.lhs = rewrite_expr(arena, c->lhs, prefix, port_aliases, env);
            pr.rhs = rewrite_expr(arena, c->rhs, prefix, port_aliases, env);
            pr.name = proc_name(arena, prefix ? prefix : m->name, "assign", nl->nprocs);
            add_proc(arena, nl, pr);
            break;
        }
        case VS_ALWAYS: {
            const vs_always_t *al = (const vs_always_t *)it;
            vs_process_t pr = {0};
            pr.kind = VS_PROC_ALWAYS;
            pr.stmt = rewrite_stmt(arena, al->stmt, prefix, port_aliases, env);
            pr.edge_sig = -1;
            pr.name = proc_name(arena, prefix ? prefix : m->name, "always", nl->nprocs);
            vs_node_t *ev = rewrite_event(arena, al->event, prefix, port_aliases, env);
            if (ev && ev->kind == VS_EVENT_CONTROL) {
                const vs_event_control_t *ec = (const vs_event_control_t *)ev;
                if (ec->star) {
                    pr.level_sensitive = 1;
                } else if (ec->expr && ec->expr->kind == VS_EVENT_EDGE) {
                    const vs_event_edge_t *ee = (const vs_event_edge_t *)ec->expr;
                    pr.edge = ee->edge;
                    pr.edge_sig = vs_netlist_find_sig(nl, ee->name);
                    if (pr.edge_sig < 0) {
                        add_sig(arena, nl, ee->name, 1, 0, VS_DIR_NONE);
                        pr.edge_sig = vs_netlist_find_sig(nl, ee->name);
                    }
                } else {
                    pr.level_sensitive = 1;
                }
            } else {
                pr.level_sensitive = 1;
            }
            add_proc(arena, nl, pr);
            break;
        }
        case VS_INITIAL: {
            const vs_initial_t *ini = (const vs_initial_t *)it;
            vs_process_t pr = {0};
            pr.kind = VS_PROC_INITIAL;
            pr.stmt = rewrite_stmt(arena, ini->stmt, prefix, port_aliases, env);
            pr.edge_sig = -1;
            pr.name = proc_name(arena, prefix ? prefix : m->name, "initial", nl->nprocs);
            add_proc(arena, nl, pr);
            break;
        }
        case VS_INSTANCE: {
            const vs_instance_t *inst = (const vs_instance_t *)it;
            if (elab_instance(arena, diag, design, nl, inst, prefix, env)) {
                return -1;
            }
            break;
        }
        default:
            break;
        }
    }
    return 0;
}

vs_netlist_t *vs_elab_flat(vs_arena_t *arena, vs_diag_t *diag, const vs_design_t *design) {
    if (!arena || !diag || !design || !design->modules) {
        vs_loc_t loc = {0};
        vs_diag_error(diag, loc, "no module to elaborate");
        return NULL;
    }

    const vs_module_t *m = design->modules;
    vs_param_env_t env;
    if (build_param_env(arena, diag, m, &env)) {
        return NULL;
    }

    vs_netlist_t *nl = vs_arena_alloc(arena, sizeof(*nl));
    memset(nl, 0, sizeof(*nl));
    nl->module_name = m->name;
    nl->annos = design->annos;
    copy_params_to_netlist(arena, nl, &env);

    if (elab_module_into(arena, diag, design, m, nl, NULL, &env, NULL)) {
        return NULL;
    }
    return nl;
}
