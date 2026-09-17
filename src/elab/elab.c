#include "vs/elab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expr_as_int(const vs_expr_t *e, int *out) {
    if (!e || e->base.kind != VS_EXPR_NUMBER || !out) {
        return -1;
    }
    const vs_expr_number_t *n = (const vs_expr_number_t *)e;
    const char *s = n->text;
    if (!s) {
        return -1;
    }
    /* Support plain decimal or sized like 4'd0 / 4'hA */
    const char *p = strchr(s, '\'');
    if (!p) {
        *out = (int)strtol(s, NULL, 10);
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
    *out = (int)strtol(digits, NULL, b);
    return 0;
}

static int range_width(const vs_range_t *r) {
    if (!r) {
        return 1;
    }
    int msb = 0, lsb = 0;
    if (expr_as_int(r->msb, &msb) || expr_as_int(r->lsb, &lsb)) {
        return 1;
    }
    return (msb >= lsb) ? (msb - lsb + 1) : (lsb - msb + 1);
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

static void add_names(vs_arena_t *a, vs_netlist_t *nl, vs_expr_t *names, vs_range_t *range, int is_reg,
                      vs_port_dir_t dir) {
    int w = range_width(range);
    for (vs_expr_t *e = names; e; e = (vs_expr_t *)e->base.next) {
        if (e->base.kind != VS_EXPR_IDENT) {
            continue;
        }
        const vs_expr_ident_t *id = (const vs_expr_ident_t *)e;
        add_sig(a, nl, id->name, w, is_reg, dir);
    }
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

vs_netlist_t *vs_elab_flat(vs_arena_t *arena, vs_diag_t *diag, const vs_design_t *design) {
    if (!arena || !diag || !design || !design->modules) {
        vs_loc_t loc = {0};
        vs_diag_error(diag, loc, "no module to elaborate");
        return NULL;
    }

    const vs_module_t *m = design->modules;
    vs_netlist_t *nl = vs_arena_alloc(arena, sizeof(*nl));
    nl->module_name = m->name;

    for (const vs_port_t *p = m->ports; p; p = (const vs_port_t *)p->base.next) {
        int is_reg = (p->dir == VS_DIR_OUTPUT);
        add_sig(arena, nl, p->name, range_width(p->range), is_reg, p->dir);
    }

    for (const vs_item_t *it = m->items; it; it = (const vs_item_t *)it->base.next) {
        switch (it->base.kind) {
        case VS_PORT_DECL: {
            const vs_port_decl_t *d = (const vs_port_decl_t *)it;
            add_names(arena, nl, d->names, d->range, 0, d->dir);
            break;
        }
        case VS_NET_DECL: {
            const vs_net_decl_t *d = (const vs_net_decl_t *)it;
            add_names(arena, nl, d->names, d->range, 0, VS_DIR_NONE);
            break;
        }
        case VS_REG_DECL: {
            const vs_reg_decl_t *d = (const vs_reg_decl_t *)it;
            add_names(arena, nl, d->names, d->range, 1, VS_DIR_NONE);
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
