#include "vs/anno.h"
#include "vs/sim.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VS_MAX_PENDING 256
#define VS_MAX_WAKE 64
#define VS_MAX_THREADS 16

typedef struct {
    int sig;
    uint64_t val;
    int is_nba;
    int proc_id;
} vs_pending_t;

typedef struct {
    vs_pending_t items[VS_MAX_PENDING];
    int n;
} vs_pending_buf_t;

typedef struct {
    uint64_t *vals;
    uint64_t *prev;
    int *changed;
    int nsigs;
    vs_netlist_t *nl;
    vs_pending_buf_t *locals; /* per thread */
    int nthreads;
    FILE *trace;
    pthread_mutex_t trace_mu;
    int verbose;
    uint64_t time;
    int delta;
} vs_sim_t;

typedef struct {
    vs_sim_t *sim;
    int tid;
    int *wake;
    int nwake;
} vs_worker_arg_t;

static void trace_event(vs_sim_t *sim, int tid, const char *op, const char *extra_fmt, ...) {
    if (!sim->trace && !sim->verbose) {
        return;
    }
    char extra[256];
    extra[0] = '\0';
    if (extra_fmt && extra_fmt[0]) {
        va_list ap;
        va_start(ap, extra_fmt);
        vsnprintf(extra, sizeof extra, extra_fmt, ap);
        va_end(ap);
    }
    char line[512];
    if (extra[0]) {
        snprintf(line, sizeof line, "{\"t\":%llu,\"d\":%d,\"tid\":%d,\"op\":\"%s\",%s}\n",
                 (unsigned long long)sim->time, sim->delta, tid, op, extra);
    } else {
        snprintf(line, sizeof line, "{\"t\":%llu,\"d\":%d,\"tid\":%d,\"op\":\"%s\"}\n",
                 (unsigned long long)sim->time, sim->delta, tid, op);
    }
    if (sim->verbose) {
        fputs(line, stderr);
    }
    if (sim->trace) {
        pthread_mutex_lock(&sim->trace_mu);
        fputs(line, sim->trace);
        fflush(sim->trace);
        pthread_mutex_unlock(&sim->trace_mu);
    }
}

static int parse_number(const char *s, uint64_t *out) {
    if (!s || !out) {
        return -1;
    }
    const char *p = strchr(s, '\'');
    if (!p) {
        *out = (uint64_t)strtoull(s, NULL, 10);
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
    *out = (uint64_t)strtoull(digits, NULL, b);
    return 0;
}

static uint64_t mask_w(int width) {
    if (width >= 64) {
        return ~0ULL;
    }
    if (width <= 0) {
        return 1;
    }
    return (1ULL << width) - 1ULL;
}

static int find_sig(vs_sim_t *sim, const char *name) {
    return vs_netlist_find_sig(sim->nl, name);
}

static uint64_t eval_expr(vs_sim_t *sim, const vs_expr_t *e);

static uint64_t eval_expr(vs_sim_t *sim, const vs_expr_t *e) {
    if (!e) {
        return 0;
    }
    switch (e->base.kind) {
    case VS_EXPR_IDENT: {
        const vs_expr_ident_t *id = (const vs_expr_ident_t *)e;
        int si = find_sig(sim, id->name);
        return si >= 0 ? sim->vals[si] : 0;
    }
    case VS_EXPR_NUMBER: {
        const vs_expr_number_t *n = (const vs_expr_number_t *)e;
        uint64_t v = 0;
        parse_number(n->text, &v);
        return v;
    }
    case VS_EXPR_UNARY: {
        const vs_expr_unary_t *u = (const vs_expr_unary_t *)e;
        uint64_t v = eval_expr(sim, u->expr);
        switch (u->op) {
        case VS_UOP_NOT:
        case VS_UOP_LNOT:
            return !v;
        case VS_UOP_MINUS:
            return (uint64_t)(-(int64_t)v);
        case VS_UOP_PLUS:
            return v;
        default:
            return v;
        }
    }
    case VS_EXPR_BINARY: {
        const vs_expr_binary_t *b = (const vs_expr_binary_t *)e;
        uint64_t l = eval_expr(sim, b->lhs);
        uint64_t r = eval_expr(sim, b->rhs);
        switch (b->op) {
        case VS_BOP_ADD:
            return l + r;
        case VS_BOP_SUB:
            return l - r;
        case VS_BOP_MUL:
            return l * r;
        case VS_BOP_AND:
            return l & r;
        case VS_BOP_OR:
            return l | r;
        case VS_BOP_XOR:
            return l ^ r;
        case VS_BOP_EQ:
            return l == r;
        case VS_BOP_NEQ:
            return l != r;
        case VS_BOP_LT:
            return l < r;
        case VS_BOP_LE:
            return l <= r;
        case VS_BOP_GT:
            return l > r;
        case VS_BOP_GE:
            return l >= r;
        case VS_BOP_LAND:
            return (l != 0) && (r != 0);
        case VS_BOP_LOR:
            return (l != 0) || (r != 0);
        default:
            return 0;
        }
    }
    case VS_EXPR_SELECT: {
        const vs_expr_select_t *s = (const vs_expr_select_t *)e;
        uint64_t v = eval_expr(sim, s->expr);
        uint64_t i = eval_expr(sim, s->index);
        return (v >> i) & 1ULL;
    }
    case VS_EXPR_PART: {
        const vs_expr_part_t *p = (const vs_expr_part_t *)e;
        uint64_t v = eval_expr(sim, p->expr);
        uint64_t msb = eval_expr(sim, p->msb);
        uint64_t lsb = eval_expr(sim, p->lsb);
        uint64_t hi = msb > lsb ? msb : lsb;
        uint64_t lo = msb > lsb ? lsb : msb;
        return (v >> lo) & mask_w((int)(hi - lo + 1));
    }
    default:
        return 0;
    }
}

static int lvalue_sig(vs_sim_t *sim, const vs_expr_t *e) {
    if (!e) {
        return -1;
    }
    if (e->base.kind == VS_EXPR_IDENT) {
        return find_sig(sim, ((const vs_expr_ident_t *)e)->name);
    }
    if (e->base.kind == VS_EXPR_SELECT || e->base.kind == VS_EXPR_PART) {
        /* For lite: only whole-signal NBA on ident; selects write whole for now */
        const vs_expr_t *base = e->base.kind == VS_EXPR_SELECT
                                    ? ((const vs_expr_select_t *)e)->expr
                                    : ((const vs_expr_part_t *)e)->expr;
        return lvalue_sig(sim, base);
    }
    return -1;
}

static void pending_push(vs_pending_buf_t *buf, int sig, uint64_t val, int is_nba, int proc_id) {
    if (buf->n >= VS_MAX_PENDING || sig < 0) {
        return;
    }
    buf->items[buf->n].sig = sig;
    buf->items[buf->n].val = val;
    buf->items[buf->n].is_nba = is_nba;
    buf->items[buf->n].proc_id = proc_id;
    buf->n++;
}

static void exec_stmt(vs_sim_t *sim, int tid, int proc_id, const vs_stmt_t *st);

static void exec_stmt(vs_sim_t *sim, int tid, int proc_id, const vs_stmt_t *st) {
    if (!st) {
        return;
    }
    vs_pending_buf_t *buf = &sim->locals[tid];
    switch (st->base.kind) {
    case VS_BLOCK: {
        const vs_block_t *b = (const vs_block_t *)st;
        for (const vs_stmt_t *s = b->stmts; s; s = (const vs_stmt_t *)s->base.next) {
            exec_stmt(sim, tid, proc_id, s);
        }
        break;
    }
    case VS_IF: {
        const vs_if_t *i = (const vs_if_t *)st;
        if (eval_expr(sim, i->cond)) {
            exec_stmt(sim, tid, proc_id, i->then_stmt);
        } else if (i->else_stmt) {
            exec_stmt(sim, tid, proc_id, i->else_stmt);
        }
        break;
    }
    case VS_NBA: {
        const vs_assign_stmt_t *a = (const vs_assign_stmt_t *)st;
        int sig = lvalue_sig(sim, a->lhs);
        uint64_t val = eval_expr(sim, a->rhs);
        if (sig >= 0) {
            val &= mask_w(sim->nl->sigs[sig].width);
            pending_push(buf, sig, val, 1, proc_id);
            trace_event(sim, tid, "nba", "\"sig\":\"%s\",\"val\":\"%llu\",\"proc\":\"%s\"",
                        sim->nl->sigs[sig].name, (unsigned long long)val,
                        sim->nl->procs[proc_id].name);
        }
        break;
    }
    case VS_BLOCKING_ASSIGN: {
        const vs_assign_stmt_t *a = (const vs_assign_stmt_t *)st;
        int sig = lvalue_sig(sim, a->lhs);
        uint64_t val = eval_expr(sim, a->rhs);
        if (sig >= 0) {
            val &= mask_w(sim->nl->sigs[sig].width);
            pending_push(buf, sig, val, 0, proc_id);
            /* Apply blocking immediately to local view for same process */
            sim->vals[sig] = val;
            trace_event(sim, tid, "ba", "\"sig\":\"%s\",\"val\":\"%llu\",\"proc\":\"%s\"",
                        sim->nl->sigs[sig].name, (unsigned long long)val,
                        sim->nl->procs[proc_id].name);
        }
        break;
    }
    default:
        break;
    }
}

static void eval_proc(vs_sim_t *sim, int tid, int proc_id) {
    vs_process_t *p = &sim->nl->procs[proc_id];
    trace_event(sim, tid, "eval", "\"proc\":\"%s\"", p->name);
    if (p->kind == VS_PROC_ASSIGN) {
        int sig = lvalue_sig(sim, p->lhs);
        uint64_t val = eval_expr(sim, p->rhs);
        if (sig >= 0) {
            val &= mask_w(sim->nl->sigs[sig].width);
            pending_push(&sim->locals[tid], sig, val, 0, proc_id);
            sim->vals[sig] = val;
            trace_event(sim, tid, "ba", "\"sig\":\"%s\",\"val\":\"%llu\",\"proc\":\"%s\"",
                        sim->nl->sigs[sig].name, (unsigned long long)val, p->name);
        }
        return;
    }
    if (p->stmt) {
        exec_stmt(sim, tid, proc_id, p->stmt);
    }
}

static void *worker_main(void *arg) {
    vs_worker_arg_t *wa = (vs_worker_arg_t *)arg;
    vs_sim_t *sim = wa->sim;
    for (int i = wa->tid; i < wa->nwake; i += sim->nthreads) {
        eval_proc(sim, wa->tid, wa->wake[i]);
    }
    return NULL;
}

static void run_wake_parallel(vs_sim_t *sim, int *wake, int nwake) {
    if (nwake <= 0) {
        return;
    }
    /* Sort wake by proc id for determinism of dispatch order across threads */
    for (int i = 0; i < nwake; i++) {
        for (int j = i + 1; j < nwake; j++) {
            if (wake[j] < wake[i]) {
                int t = wake[i];
                wake[i] = wake[j];
                wake[j] = t;
            }
        }
    }
    for (int t = 0; t < sim->nthreads; t++) {
        sim->locals[t].n = 0;
    }
    if (sim->nthreads <= 1 || nwake == 1) {
        for (int i = 0; i < nwake; i++) {
            eval_proc(sim, 0, wake[i]);
        }
        return;
    }
    pthread_t th[VS_MAX_THREADS];
    vs_worker_arg_t args[VS_MAX_THREADS];
    int n = sim->nthreads;
    if (n > VS_MAX_THREADS) {
        n = VS_MAX_THREADS;
    }
    for (int t = 0; t < n; t++) {
        args[t].sim = sim;
        args[t].tid = t;
        args[t].wake = wake;
        args[t].nwake = nwake;
        pthread_create(&th[t], NULL, worker_main, &args[t]);
    }
    for (int t = 0; t < n; t++) {
        pthread_join(th[t], NULL);
    }
}

static int merge_pending(vs_sim_t *sim, int nba_phase) {
    /* Collect all pending of type, sort by proc_id then apply in order */
    vs_pending_t all[VS_MAX_PENDING * VS_MAX_THREADS];
    int n = 0;
    for (int t = 0; t < sim->nthreads; t++) {
        for (int i = 0; i < sim->locals[t].n; i++) {
            vs_pending_t *p = &sim->locals[t].items[i];
            if ((nba_phase && p->is_nba) || (!nba_phase && !p->is_nba)) {
                if (n < (int)(sizeof all / sizeof all[0])) {
                    all[n++] = *p;
                }
            }
        }
    }
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (all[j].proc_id < all[i].proc_id ||
                (all[j].proc_id == all[i].proc_id && all[j].sig < all[i].sig)) {
                vs_pending_t tmp = all[i];
                all[i] = all[j];
                all[j] = tmp;
            }
        }
    }
    int any = 0;
    int last_sig = -1;
    uint64_t last_val = 0;
    int last_proc = -1;
    for (int i = 0; i < n; i++) {
        int sig = all[i].sig;
        uint64_t val = all[i].val;
        if (sig == last_sig && val != last_val) {
            trace_event(sim, 0, "warn",
                        "\"msg\":\"conflict on %s procs %d and %d\"",
                        sim->nl->sigs[sig].name, last_proc, all[i].proc_id);
        }
        if (sim->vals[sig] != val) {
            sim->vals[sig] = val;
            sim->changed[sig] = 1;
            any = 1;
            if (nba_phase) {
                trace_event(sim, 0, "commit", "\"sig\":\"%s\",\"val\":\"%llu\"",
                            sim->nl->sigs[sig].name, (unsigned long long)val);
            }
        }
        last_sig = sig;
        last_val = val;
        last_proc = all[i].proc_id;
    }
    /* Clear consumed from locals */
    if (nba_phase) {
        for (int t = 0; t < sim->nthreads; t++) {
            int w = 0;
            for (int i = 0; i < sim->locals[t].n; i++) {
                if (!sim->locals[t].items[i].is_nba) {
                    sim->locals[t].items[w++] = sim->locals[t].items[i];
                }
            }
            sim->locals[t].n = w;
        }
    } else {
        for (int t = 0; t < sim->nthreads; t++) {
            int w = 0;
            for (int i = 0; i < sim->locals[t].n; i++) {
                if (sim->locals[t].items[i].is_nba) {
                    sim->locals[t].items[w++] = sim->locals[t].items[i];
                }
            }
            sim->locals[t].n = w;
        }
    }
    return any;
}

static void collect_edge_wake(vs_sim_t *sim, int *wake, int *nwake) {
    *nwake = 0;
    for (int i = 0; i < sim->nl->nprocs; i++) {
        vs_process_t *p = &sim->nl->procs[i];
        if (p->kind == VS_PROC_INITIAL) {
            continue;
        }
        if (p->level_sensitive || p->kind == VS_PROC_ASSIGN) {
            int wake_it = 0;
            for (int s = 0; s < sim->nsigs; s++) {
                if (sim->changed[s]) {
                    wake_it = 1;
                    break;
                }
            }
            if (wake_it && *nwake < VS_MAX_WAKE) {
                wake[(*nwake)++] = i;
            }
            continue;
        }
        if (p->edge_sig >= 0 && sim->changed[p->edge_sig]) {
            uint64_t now = sim->vals[p->edge_sig] & 1ULL;
            uint64_t was = sim->prev[p->edge_sig] & 1ULL;
            int fire = 0;
            if (p->edge == VS_EDGE_POSEDGE && was == 0 && now == 1) {
                fire = 1;
            }
            if (p->edge == VS_EDGE_NEGEDGE && was == 1 && now == 0) {
                fire = 1;
            }
            if (fire && *nwake < VS_MAX_WAKE) {
                wake[(*nwake)++] = i;
            }
        }
    }
}

static void snapshot_prev(vs_sim_t *sim) {
    memcpy(sim->prev, sim->vals, (size_t)sim->nsigs * sizeof(uint64_t));
    memset(sim->changed, 0, (size_t)sim->nsigs * sizeof(int));
}

static void set_sig(vs_sim_t *sim, int sig, uint64_t val, const char *op) {
    if (sig < 0) {
        return;
    }
    val &= mask_w(sim->nl->sigs[sig].width);
    if (sim->vals[sig] != val) {
        sim->prev[sig] = sim->vals[sig];
        sim->vals[sig] = val;
        sim->changed[sig] = 1;
        trace_event(sim, 0, op, "\"sig\":\"%s\",\"val\":\"%llu\"", sim->nl->sigs[sig].name,
                    (unsigned long long)val);
    }
}

static void format_val(char *buf, size_t n, uint64_t v, int width) {
    if (width <= 1) {
        snprintf(buf, n, "%llu", (unsigned long long)(v & 1ULL));
    } else {
        snprintf(buf, n, "%llu", (unsigned long long)(v & mask_w(width)));
    }
}

static const char *dir_str(vs_port_dir_t d) {
    switch (d) {
    case VS_DIR_INPUT:
        return "input";
    case VS_DIR_OUTPUT:
        return "output";
    case VS_DIR_INOUT:
        return "inout";
    default:
        return "internal";
    }
}

static void trace_meta(vs_sim_t *sim) {
    if (!sim->trace && !sim->verbose) {
        return;
    }
    char ports[2048];
    size_t off = 0;
    ports[0] = '\0';
    off += (size_t)snprintf(ports + off, sizeof ports - off, "[");
    for (int i = 0; i < sim->nl->nsigs; i++) {
        vs_signal_t *s = &sim->nl->sigs[i];
        if (i > 0) {
            off += (size_t)snprintf(ports + off, sizeof ports - off, ",");
        }
        off += (size_t)snprintf(ports + off, sizeof ports - off,
                                "{\"name\":\"%s\",\"dir\":\"%s\",\"width\":%d,\"reg\":%s}", s->name,
                                dir_str(s->dir), s->width, s->is_reg ? "true" : "false");
        if (off >= sizeof ports - 64) {
            break;
        }
    }
    snprintf(ports + off, sizeof ports - off, "]");

    char procs[1024];
    size_t po = 0;
    procs[0] = '\0';
    po += (size_t)snprintf(procs + po, sizeof procs - po, "[");
    for (int i = 0; i < sim->nl->nprocs; i++) {
        if (i > 0) {
            po += (size_t)snprintf(procs + po, sizeof procs - po, ",");
        }
        const char *k = "always";
        if (sim->nl->procs[i].kind == VS_PROC_ASSIGN) {
            k = "assign";
        } else if (sim->nl->procs[i].kind == VS_PROC_INITIAL) {
            k = "initial";
        }
        po += (size_t)snprintf(procs + po, sizeof procs - po, "{\"name\":\"%s\",\"kind\":\"%s\"}",
                               sim->nl->procs[i].name, k);
        if (po >= sizeof procs - 64) {
            break;
        }
    }
    snprintf(procs + po, sizeof procs - po, "]");

    char anno_buf[12288];
    size_t anno_off = 0;
    anno_buf[0] = '\0';
    if (vs_anno_append_meta_json(anno_buf, sizeof anno_buf, &anno_off, sim->nl->annos) != 0) {
        /* keep going with truncated / empty anno fragment */
        anno_buf[0] = '\0';
    }

    char line[16384];
    snprintf(line, sizeof line,
             "{\"t\":0,\"d\":0,\"tid\":0,\"op\":\"meta\",\"module\":\"%s\",\"ports\":%s,\"procs\":%s%s}\n",
             sim->nl->module_name ? sim->nl->module_name : "top", ports, procs, anno_buf);
    if (sim->verbose) {
        fputs(line, stderr);
    }
    if (sim->trace) {
        pthread_mutex_lock(&sim->trace_mu);
        fputs(line, sim->trace);
        fflush(sim->trace);
        pthread_mutex_unlock(&sim->trace_mu);
    }
}

int vs_sim_run(vs_arena_t *arena, vs_diag_t *diag, vs_netlist_t *nl, const vs_sim_opts_t *opts) {
    if (!arena || !diag || !nl || !opts) {
        return 1;
    }

    vs_sim_t sim;
    memset(&sim, 0, sizeof sim);
    sim.nl = nl;
    sim.nsigs = nl->nsigs;
    sim.nthreads = opts->threads > 0 ? opts->threads : 1;
    if (sim.nthreads > VS_MAX_THREADS) {
        sim.nthreads = VS_MAX_THREADS;
    }
    sim.verbose = opts->verbose;
    sim.vals = vs_arena_alloc(arena, (size_t)sim.nsigs * sizeof(uint64_t));
    sim.prev = vs_arena_alloc(arena, (size_t)sim.nsigs * sizeof(uint64_t));
    sim.changed = vs_arena_alloc(arena, (size_t)sim.nsigs * sizeof(int));
    sim.locals = vs_arena_alloc(arena, (size_t)sim.nthreads * sizeof(vs_pending_buf_t));
    pthread_mutex_init(&sim.trace_mu, NULL);

    if (opts->trace_path) {
        sim.trace = fopen(opts->trace_path, "w");
        if (!sim.trace) {
            vs_loc_t loc = {.path = opts->trace_path, .first_line = 1, .first_column = 1};
            vs_diag_error(diag, loc, "cannot open trace file");
            pthread_mutex_destroy(&sim.trace_mu);
            return 1;
        }
    }

    int clk = opts->clock_name ? vs_netlist_find_sig(nl, opts->clock_name) : -1;
    int rst = opts->reset_name ? vs_netlist_find_sig(nl, opts->reset_name) : -1;
    int period = opts->clock_period > 0 ? opts->clock_period : 10;
    int half = period / 2;
    if (half < 1) {
        half = 1;
    }

    sim.time = 0;
    sim.delta = 0;
    trace_meta(&sim);

    /* Run initials once at t=0 */
    int wake[VS_MAX_WAKE];
    int nwake = 0;
    for (int i = 0; i < nl->nprocs; i++) {
        if (nl->procs[i].kind == VS_PROC_INITIAL && nwake < VS_MAX_WAKE) {
            wake[nwake++] = i;
        }
    }
    run_wake_parallel(&sim, wake, nwake);
    merge_pending(&sim, 0);
    merge_pending(&sim, 1);

    for (uint64_t t = 0; t <= opts->until_time; t++) {
        sim.time = t;
        sim.delta = 0;
        snapshot_prev(&sim);

        if (rst >= 0) {
            uint64_t rv = (opts->reset_cycles > 0 && (int)t < opts->reset_cycles) ? 1 : 0;
            set_sig(&sim, rst, rv, "reset");
        }

        if (opts->forces && opts->nforces > 0) {
            for (int fi = 0; fi < opts->nforces; fi++) {
                int si = vs_netlist_find_sig(nl, opts->forces[fi].name);
                if (si >= 0) {
                    set_sig(&sim, si, opts->forces[fi].value, "force");
                }
            }
        }

        if (clk >= 0 && period > 0) {
            uint64_t phase = t % (uint64_t)period;
            uint64_t cv = (phase < (uint64_t)half) ? 1 : 0;
            set_sig(&sim, clk, cv, "clock");
        }

        /* Delta loop */
        for (int d = 0; d < 64; d++) {
            sim.delta = d;
            nwake = 0;
            collect_edge_wake(&sim, wake, &nwake);
            /* Also first delta after time advance may need level assigns */
            if (d == 0) {
                for (int i = 0; i < nl->nprocs; i++) {
                    if (nl->procs[i].kind == VS_PROC_ASSIGN || nl->procs[i].level_sensitive) {
                        int already = 0;
                        for (int k = 0; k < nwake; k++) {
                            if (wake[k] == i) {
                                already = 1;
                                break;
                            }
                        }
                        if (!already && nwake < VS_MAX_WAKE) {
                            wake[nwake++] = i;
                        }
                    }
                }
            }
            if (nwake == 0) {
                /* Still commit any NBA left */
                int had = 0;
                for (int th = 0; th < sim.nthreads; th++) {
                    for (int i = 0; i < sim.locals[th].n; i++) {
                        if (sim.locals[th].items[i].is_nba) {
                            had = 1;
                        }
                    }
                }
                if (had) {
                    snapshot_prev(&sim);
                    merge_pending(&sim, 1);
                    continue;
                }
                break;
            }
            /* Clear changed after scheduling edges based on them — keep until after eval */
            run_wake_parallel(&sim, wake, nwake);
            merge_pending(&sim, 0); /* blocking already applied mostly */
            snapshot_prev(&sim);
            memset(sim.changed, 0, (size_t)sim.nsigs * sizeof(int));
            merge_pending(&sim, 1); /* NBA commit sets changed */
        }
    }

    trace_event(&sim, 0, "done", "\"msg\":\"finished\"");

    if (opts->out_watch_vals && opts->watch && opts->nwatch > 0) {
        for (int i = 0; i < opts->nwatch; i++) {
            int si = vs_netlist_find_sig(nl, opts->watch[i]);
            char tmp[64];
            if (si >= 0) {
                format_val(tmp, sizeof tmp, sim.vals[si], nl->sigs[si].width);
            } else {
                snprintf(tmp, sizeof tmp, "?");
            }
            size_t n = strlen(tmp) + 1;
            char *s = vs_arena_alloc(arena, n);
            memcpy(s, tmp, n);
            opts->out_watch_vals[i] = s;
        }
    }

    if (sim.trace) {
        fclose(sim.trace);
    }
    pthread_mutex_destroy(&sim.trace_mu);
    return 0;
}
