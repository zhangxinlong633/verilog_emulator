#ifndef VS_SIM_H
#define VS_SIM_H

#include "vs/arena.h"
#include "vs/diag.h"
#include "vs/elab.h"

#include <stdint.h>

typedef struct vs_sim_opts {
    int threads;
    uint64_t until_time;
    const char *clock_name;
    int clock_period;
    const char *reset_name;
    int reset_cycles;
    const char *trace_path;
    int verbose;
    const char **watch;
    int nwatch;
    /* Optional: fill final watched values (length nwatch) as decimal strings into arena */
    char **out_watch_vals;
} vs_sim_opts_t;

/* Run simulation. Returns 0 on success. */
int vs_sim_run(vs_arena_t *arena, vs_diag_t *diag, vs_netlist_t *nl, const vs_sim_opts_t *opts);

#endif
