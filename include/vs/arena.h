#ifndef VS_ARENA_H
#define VS_ARENA_H

#include <stddef.h>

typedef struct vs_arena vs_arena_t;

vs_arena_t *vs_arena_create(void);
void vs_arena_destroy(vs_arena_t *a);
void *vs_arena_alloc(vs_arena_t *a, size_t n);

#endif
