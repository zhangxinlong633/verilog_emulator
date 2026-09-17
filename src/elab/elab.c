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

static int range_width(vs_diag_t *diag, const vs_param_env_t *env, const vs_range_t *r, int *out) {
    if (!out) {
        return -1;
    }
    if (!r) {
        *out = 1;
        return 0;
    }
    int64_t msb = 0, lsb = 0;
    if (eval_const_expr(diag, env, r->msb, &msb) || eval_const_expr(diag, env, r->lsb, &lsb)) {
        return -1;
    }
    *out = (msb >= lsb) ? (int)(msb - lsb + 1) : (int)(lsb - msb + 1);
    if (*out <= 0) {
        vs_diag_error(diag, r->base.loc, "invalid packed range width");
        return -1;
    }
    return 0;
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

static int add_names(vs_arena_t *a, vs_diag_t *diag, vs_param_env_t *env, vs_netlist_t *nl,
                     vs_expr_t *names, vs_range_t *range, int is_reg, vs_port_dir_t dir) {
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
    nl->module_name = m->name;
    nl->annos = design->annos;

    for (const vs_port_t *p = m->ports; p; p = (const vs_port_t *)p->base.next) {
        int is_reg = (p->dir == VS_DIR_OUTPUT);
        int w = 0;
        if (range_width(diag, &env, p->range, &w)) {
            return NULL;
        }
        add_sig(arena, nl, p->name, w, is_reg, p->dir);
    }

    for (const vs_item_t *it = m->items; it; it = (const vs_item_t *)it->base.next) {
        switch (it->base.kind) {
        case VS_PORT_DECL: {
            const vs_port_decl_t *d = (const vs_port_decl_t *)it;
            if (add_names(arena, diag, &env, nl, d->names, d->range, 0, d->dir)) {
                return NULL;
            }
            break;
        }
        case VS_NET_DECL: {
            const vs_net_decl_t *d = (const vs_net_decl_t *)it;
            if (add_names(arena, diag, &env, nl, d->names, d->range, 0, VS_DIR_NONE)) {
                return NULL;
            }
            break;
        }
        case VS_REG_DECL: {
            const vs_reg_decl_t *d = (const vs_reg_decl_t *)it;
            if (add_names(arena, diag, &env, nl, d->names, d->range, 1, VS_DIR_NONE)) {
                return NULL;
            }
            break;
        }
        case VS_CONT_ASSIGN: {
            const vs_cont_assign_t *c = (const vs_cont_assign_t *)it;
            vs_process_t pr = {0};
            pr.kind = VS_PROC_ASSIGN;
            pr.level_sensitive = 1;
            pr.edge_sig = -1;
            pr.lhs = c->lhs;
            pr.rhs = c->rhs;
            pr.name = proc_name(arena, m->name, "assign", nl->nprocs);
            add_proc(arena, nl, pr);
            break;
        }
        case VS_ALWAYS: {
            const vs_always_t *al = (const vs_always_t *)it;
            vs_process_t pr = {0};
            pr.kind = VS_PROC_ALWAYS;
            pr.stmt = al->stmt;
            pr.edge_sig = -1;
            pr.name = proc_name(arena, m->name, "always", nl->nprocs);
            if (al->event && al->event->kind == VS_EVENT_CONTROL) {
                const vs_event_control_t *ec = (const vs_event_control_t *)al->event;
                if (ec->star) {
                    pr.level_sensitive = 1;
                } else if (ec->expr && ec->expr->kind == VS_EVENT_EDGE) {
                    const vs_event_edge_t *ee = (const vs_event_edge_t *)ec->expr;
                    pr.edge = ee->edge;
                    pr.edge_sig = vs_netlist_find_sig(nl, ee->name);
                    if (pr.edge_sig < 0) {
                        /* edge signal may be port not yet widened; add */
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
            pr.stmt = ini->stmt;
            pr.edge_sig = -1;
            pr.name = proc_name(arena, m->name, "initial", nl->nprocs);
            add_proc(arena, nl, pr);
            break;
        }
        default:
            break;
        }
    }

    /* Mark output regs from port list + reg_decl already handled */
    return nl;
}
