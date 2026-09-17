#include "vs/arena.h"

#include <stdlib.h>
#include <string.h>

#define VS_ARENA_CHUNK_SIZE (64 * 1024)

typedef struct vs_arena_chunk {
    struct vs_arena_chunk *next;
    size_t used;
    size_t cap;
    unsigned char data[];
} vs_arena_chunk_t;

struct vs_arena {
    vs_arena_chunk_t *head;
};

static vs_arena_chunk_t *vs_arena_new_chunk(size_t need) {
    size_t cap = VS_ARENA_CHUNK_SIZE;
    if (need > cap) {
        cap = need;
    }
    vs_arena_chunk_t *c = malloc(sizeof(*c) + cap);
    if (!c) {
        return NULL;
    }
    c->next = NULL;
    c->used = 0;
    c->cap = cap;
    return c;
}

vs_arena_t *vs_arena_create(void) {
    vs_arena_t *a = calloc(1, sizeof(*a));
    if (!a) {
        return NULL;
    }
    a->head = vs_arena_new_chunk(0);
    if (!a->head) {
        free(a);
        return NULL;
    }
    return a;
}

void vs_arena_destroy(vs_arena_t *a) {
    if (!a) {
        return;
    }
    vs_arena_chunk_t *c = a->head;
    while (c) {
        vs_arena_chunk_t *n = c->next;
        free(c);
        c = n;
    }
    free(a);
}

void *vs_arena_alloc(vs_arena_t *a, size_t n) {
    if (!a || n == 0) {
        return NULL;
    }
    /* Align to max(sizeof(void*), 8) */
    size_t align = sizeof(void *) > 8 ? sizeof(void *) : 8;
    size_t pad = (align - (a->head->used % align)) % align;
    if (a->head->used + pad + n > a->head->cap) {
        vs_arena_chunk_t *c = vs_arena_new_chunk(n + align);
        if (!c) {
            return NULL;
        }
        c->next = a->head;
        a->head = c;
        pad = 0;
    }
    a->head->used += pad;
    void *p = a->head->data + a->head->used;
    a->head->used += n;
    memset(p, 0, n);
    return p;
}
