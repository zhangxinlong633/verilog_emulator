#include "vs/arena.h"
#include "vs/ast.h"
#include "vs/diag.h"
#include "vs/parse.h"
#include "vs/strtab.h"

#include <stdio.h>
#include <string.h>

static void usage(FILE *out) {
    fprintf(out,
            "Usage: vs [options] <file.v> [<file.v> ...]\n"
            "Options:\n"
            "  --dump-ast       Print AST to stdout (default)\n"
            "  --dump-tokens    Print token stream to stdout\n"
            "  -h, --help       Show this help\n"
            "  -V, --version    Show version\n");
}

int main(int argc, char **argv) {
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
            puts("vs 0.0.0-p0");
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
    if (!arena) {
        fprintf(stderr, "vs: out of memory\n");
        return 2;
    }
    vs_strtab_t *strtab = vs_strtab_create(arena);
    vs_diag_t *diag = vs_diag_create(arena);
    if (!strtab || !diag) {
        fprintf(stderr, "vs: out of memory\n");
        vs_arena_destroy(arena);
        return 2;
    }

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
    if (status == 0 && vs_diag_error_count(diag) > 0) {
        status = 1;
    }

    vs_arena_destroy(arena);
    return status;
}
