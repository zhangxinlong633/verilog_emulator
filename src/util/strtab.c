#include "vs/strtab.h"

#include <string.h>

#define VS_STRTAB_BUCKETS 256

typedef struct vs_strtab_entry {
    struct vs_strtab_entry *next;
    size_t len;
    char str[];
} vs_strtab_entry_t;

struct vs_strtab {
    vs_arena_t *arena;
    vs_strtab_entry_t *buckets[VS_STRTAB_BUCKETS];
};

static unsigned vs_hash_n(const char *s, size_t n) {
    unsigned h = 5381;
    for (size_t i = 0; i < n; i++) {
        h = ((h << 5) + h) + (unsigned char)s[i];
    }
    return h;
}

vs_strtab_t *vs_strtab_create(vs_arena_t *a) {
    vs_strtab_t *t = vs_arena_alloc(a, sizeof(*t));
    if (!t) {
        return NULL;
    }
    t->arena = a;
    return t;
}

const char *vs_strtab_intern_n(vs_strtab_t *t, const char *s, size_t n) {
    if (!t || !s) {
        return NULL;
    }
    unsigned idx = vs_hash_n(s, n) % VS_STRTAB_BUCKETS;
    for (vs_strtab_entry_t *e = t->buckets[idx]; e; e = e->next) {
        if (e->len == n && memcmp(e->str, s, n) == 0) {
            return e->str;
        }
    }
    vs_strtab_entry_t *e = vs_arena_alloc(t->arena, sizeof(*e) + n + 1);
    if (!e) {
        return NULL;
    }
    e->len = n;
    memcpy(e->str, s, n);
    e->str[n] = '\0';
    e->next = t->buckets[idx];
    t->buckets[idx] = e;
    return e->str;
}

const char *vs_strtab_intern(vs_strtab_t *t, const char *s) {
    if (!s) {
        return NULL;
    }
    return vs_strtab_intern_n(t, s, strlen(s));
}
