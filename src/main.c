#include "vs/anno.h"
#include "vs/arena.h"
#include "vs/ast.h"
#include "vs/diag.h"
#include "vs/elab.h"
#include "vs/parse.h"
#include "vs/sim.h"
#include "vs/strtab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#endif

static void usage(FILE *out) {
    fprintf(out,
            "Usage:\n"
            "  vs run <file.v> [options]\n"
            "  vs [--dump-ast|--dump-tokens] <file.v>...\n"
            "\n"
            "Run options:\n"
            "  --threads N          Worker threads (default: min(4, CPUs))\n"
            "  --until T            Stop time (default: 200)\n"
            "  --trace PATH         Write JSONL process dump\n"
            "  --clock name=period  Host clock (e.g. clk=10)\n"
            "  --reset name=cycles  Reset high for first cycles (e.g. rst=20)\n"
            "  --force name=value   Hold input constant (repeatable)\n"
            "  --watch a,b,q        Print final values\n"
            "  -v, --verbose        Mirror trace events to stderr\n"
            "\n"
            "Other:\n"
            "  --dump-ast           Print AST\n"
            "  --dump-tokens        Print tokens\n"
            "  -h, --help           Help\n"
            "  -V, --version        Version\n");
}

static int default_threads(void) {
    long n = 4;
#if defined(_SC_NPROCESSORS_ONLN)
    long c = sysconf(_SC_NPROCESSORS_ONLN);
    if (c > 0) {
        n = c;
    }
#endif
    if (n > 4) {
        n = 4;
    }
    if (n < 1) {
        n = 1;
    }
    return (int)n;
}

static int split_watch(vs_arena_t *a, const char *s, const char ***out, int *nout) {
    if (!s || !s[0]) {
        *out = NULL;
        *nout = 0;
        return 0;
    }
    int count = 1;
    for (const char *p = s; *p; p++) {
        if (*p == ',') {
            count++;
        }
    }
    const char **arr = vs_arena_alloc(a, (size_t)count * sizeof(char *));
    char *buf = vs_arena_alloc(a, strlen(s) + 1);
    memcpy(buf, s, strlen(s) + 1);
    int i = 0;
    char *tok = strtok(buf, ",");
    while (tok && i < count) {
        arr[i++] = tok;
        tok = strtok(NULL, ",");
    }
    *out = arr;
    *nout = i;
    return 0;
}

static int cmd_run(int argc, char **argv) {
    const char *file = NULL;
    int threads = default_threads();
    unsigned long long until = 200;
    const char *trace = NULL;
    const char *clock_name = "clk";
    int clock_period = 10;
    int have_clock = 0;
    const char *reset_name = "rst";
    int reset_cycles = 0;
    int have_reset = 0;
    const char *watch_str = NULL;
    int verbose = 0;
    vs_sim_force_t force_buf[32];
    int nforces = 0;

    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(stdout);
            return 0;
        }
        if (!strcmp(argv[i], "--threads") && i + 1 < argc) {
            threads = atoi(argv[++i]);
            if (threads < 1) {
                threads = 1;
            }
            continue;
        }
        if (!strcmp(argv[i], "--until") && i + 1 < argc) {
            until = strtoull(argv[++i], NULL, 10);
            continue;
        }
        if (!strcmp(argv[i], "--trace") && i + 1 < argc) {
            trace = argv[++i];
            continue;
        }
        if (!strcmp(argv[i], "--clock") && i + 1 < argc) {
            const char *spec = argv[++i];
            const char *eq = strchr(spec, '=');
            if (!eq) {
                fprintf(stderr, "vs: --clock expects name=period\n");
                return 2;
            }
            size_t nlen = (size_t)(eq - spec);
            char *name = malloc(nlen + 1);
            memcpy(name, spec, nlen);
            name[nlen] = '\0';
            clock_name = name;
            clock_period = atoi(eq + 1);
            have_clock = 1;
            continue;
        }
        if (!strcmp(argv[i], "--reset") && i + 1 < argc) {
            const char *spec = argv[++i];
            const char *eq = strchr(spec, '=');
            if (!eq) {
                fprintf(stderr, "vs: --reset expects name=cycles\n");
                return 2;
            }
            size_t nlen = (size_t)(eq - spec);
            char *name = malloc(nlen + 1);
            memcpy(name, spec, nlen);
            name[nlen] = '\0';
            reset_name = name;
            reset_cycles = atoi(eq + 1);
            have_reset = 1;
            continue;
        }
        if (!strcmp(argv[i], "--force") && i + 1 < argc) {
            const char *spec = argv[++i];
            const char *eq = strchr(spec, '=');
            if (!eq || nforces >= 32) {
                fprintf(stderr, "vs: --force expects name=value (max 32)\n");
                return 2;
            }
            size_t nlen = (size_t)(eq - spec);
            char *name = malloc(nlen + 1);
            memcpy(name, spec, nlen);
            name[nlen] = '\0';
            force_buf[nforces].name = name;
            force_buf[nforces].value = strtoull(eq + 1, NULL, 0);
            nforces++;
            continue;
        }
        if (!strcmp(argv[i], "--watch") && i + 1 < argc) {
            watch_str = argv[++i];
            continue;
        }
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
            verbose = 1;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "vs: unknown run option %s\n", argv[i]);
            return 2;
        }
        if (!file) {
            file = argv[i];
        }
    }

    if (!file) {
        fprintf(stderr, "vs: run requires a .v file\n");
        usage(stderr);
        return 2;
    }
    if (!have_clock) {
        have_clock = 1; /* default clk=10 if signal exists */
    }
    if (!watch_str) {
        watch_str = "q,clk,rst";
    }

    vs_arena_t *arena = vs_arena_create();
    vs_strtab_t *strtab = vs_strtab_create(arena);
    vs_diag_t *diag = vs_diag_create(arena);
    if (!arena || !strtab || !diag) {
        fprintf(stderr, "vs: out of memory\n");
        return 2;
    }

    char *paths[1] = {(char *)file};
    vs_design_t *design = NULL;
    if (vs_parse_files(arena, strtab, diag, 1, paths, &design)) {
        vs_diag_print_all(diag, stderr);
        vs_arena_destroy(arena);
        return 1;
    }
    design->annos = vs_anno_scan_file(arena, diag, file);

    vs_netlist_t *nl = vs_elab_flat(arena, diag, design);
    if (!nl) {
        vs_diag_print_all(diag, stderr);
        vs_arena_destroy(arena);
        return 1;
    }

    const char **watch = NULL;
    int nwatch = 0;
    split_watch(arena, watch_str, &watch, &nwatch);
    char **out_vals = NULL;
    if (nwatch > 0) {
        out_vals = vs_arena_alloc(arena, (size_t)nwatch * sizeof(char *));
    }

    vs_sim_opts_t opts = {0};
    opts.threads = threads;
    opts.until_time = until;
    opts.clock_name = have_clock ? clock_name : NULL;
    opts.clock_period = clock_period;
    opts.reset_name = have_reset ? reset_name : (vs_netlist_find_sig(nl, "rst") >= 0 ? "rst" : NULL);
    opts.reset_cycles = have_reset ? reset_cycles : (opts.reset_name ? 20 : 0);
    opts.trace_path = trace;
    opts.verbose = verbose;
    opts.watch = watch;
    opts.nwatch = nwatch;
    opts.out_watch_vals = out_vals;
    opts.forces = nforces > 0 ? force_buf : NULL;
    opts.nforces = nforces;

    int rc = vs_sim_run(arena, diag, nl, &opts);
    vs_diag_print_all(diag, stderr);
    if (rc != 0) {
        vs_arena_destroy(arena);
        return 1;
    }

    if (nwatch > 0 && out_vals) {
        fputs("watch:", stdout);
        for (int i = 0; i < nwatch; i++) {
            printf(" %s=%s", watch[i], out_vals[i] ? out_vals[i] : "?");
        }
        fputc('\n', stdout);
    }

    vs_arena_destroy(arena);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "run")) {
        return cmd_run(argc - 2, argv + 2);
    }

    int dump_ast = 0;
    int dump_tokens = 0;
    int file_start = -1;
    int file_count = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(stdout);
            return 0;
        }
        if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
            puts("vs 0.1.0-p2-lite-mt");
            return 0;
        }
        if (!strcmp(argv[i], "--dump-ast")) {
            dump_ast = 1;
            continue;
        }
        if (!strcmp(argv[i], "--dump-tokens")) {
            dump_tokens = 1;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "vs: unknown option %s\n", argv[i]);
            usage(stderr);
            return 2;
        }
        if (file_start < 0) {
            file_start = i;
        }
        file_count++;
    }

    if (file_count == 0) {
        usage(stderr);
        return 2;
    }
    if (!dump_ast && !dump_tokens) {
        dump_ast = 1;
    }

    vs_arena_t *arena = vs_arena_create();
    vs_strtab_t *strtab = vs_strtab_create(arena);
    vs_diag_t *diag = vs_diag_create(arena);
    int status = 0;

    if (dump_tokens) {
        for (int i = 0; i < file_count; i++) {
            if (vs_dump_tokens_file(stdout, arena, strtab, diag, argv[file_start + i])) {
                status = 1;
            }
        }
    }
    if (dump_ast) {
        vs_design_t *design = NULL;
        if (vs_parse_files(arena, strtab, diag, file_count, &argv[file_start], &design)) {
            status = 1;
        } else if (design) {
            vs_dump_ast(stdout, design);
        } else {
            status = 1;
        }
    }
    vs_diag_print_all(diag, stderr);
    vs_arena_destroy(arena);
    return status;
}
