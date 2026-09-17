#ifndef VS_PARSE_H
#define VS_PARSE_H

#include "vs/arena.h"
#include "vs/ast.h"
#include "vs/diag.h"
#include "vs/strtab.h"

#include <stdio.h>

typedef struct vs_parse_ctx {
    vs_arena_t *arena;
    vs_strtab_t *strtab;
    vs_diag_t *diag;
    vs_design_t *design;
    const char *filename;
    int column;
} vs_parse_ctx_t;

/* Returns 0 on success (no errors), 1 on parse/lex errors. */
int vs_parse_files(vs_arena_t *arena, vs_strtab_t *strtab, vs_diag_t *diag, int npaths,
                   char **paths, vs_design_t **out_design);

int vs_dump_tokens_file(FILE *out, vs_arena_t *arena, vs_strtab_t *strtab, vs_diag_t *diag,
                        const char *path);

#endif
