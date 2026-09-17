#ifndef VS_STRTAB_H
#define VS_STRTAB_H

#include "vs/arena.h"
#include <stddef.h>

typedef struct vs_strtab vs_strtab_t;

vs_strtab_t *vs_strtab_create(vs_arena_t *a);
const char *vs_strtab_intern(vs_strtab_t *t, const char *s);
const char *vs_strtab_intern_n(vs_strtab_t *t, const char *s, size_t n);

#endif
