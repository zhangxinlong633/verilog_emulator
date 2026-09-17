#ifndef VS_DIAG_H
#define VS_DIAG_H

#include "vs/arena.h"
#include <stdio.h>

typedef struct vs_loc {
    const char *path;
    int first_line;
    int first_column;
    int last_line;
    int last_column;
} vs_loc_t;

typedef struct vs_diag vs_diag_t;

vs_diag_t *vs_diag_create(vs_arena_t *a);
void vs_diag_error(vs_diag_t *d, vs_loc_t loc, const char *fmt, ...);
void vs_diag_warn(vs_diag_t *d, vs_loc_t loc, const char *fmt, ...);
unsigned vs_diag_error_count(const vs_diag_t *d);
void vs_diag_print_all(const vs_diag_t *d, FILE *out);

#endif
