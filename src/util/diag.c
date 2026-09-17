#include "vs/diag.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    VS_SEV_ERROR = 0,
    VS_SEV_WARN = 1
} vs_severity_t;

typedef struct vs_diag_msg {
    vs_loc_t loc;
    vs_severity_t sev;
    char *text; /* malloc'd so vsnprintf can size freely; freed on destroy via arena? */
} vs_diag_msg_t;

struct vs_diag {
    vs_arena_t *arena;
    vs_diag_msg_t *msgs;
    size_t count;
    size_t cap;
    unsigned errors;
};

vs_diag_t *vs_diag_create(vs_arena_t *a) {
    vs_diag_t *d = vs_arena_alloc(a, sizeof(*d));
    if (!d) {
        return NULL;
    }
    d->arena = a;
    d->msgs = NULL;
    d->count = 0;
    d->cap = 0;
    d->errors = 0;
    return d;
}

static char *vs_diag_vformat(const char *fmt, va_list ap) {
    va_list aq;
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, aq);
    va_end(aq);
    if (n < 0) {
        return NULL;
    }
    char *buf = malloc((size_t)n + 1);
    if (!buf) {
        return NULL;
    }
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    return buf;
}

static void vs_diag_push(vs_diag_t *d, vs_severity_t sev, vs_loc_t loc, const char *fmt,
                         va_list ap) {
    if (!d) {
        return;
    }
    if (d->count == d->cap) {
        size_t ncap = d->cap ? d->cap * 2 : 8;
        vs_diag_msg_t *nm = realloc(d->msgs, ncap * sizeof(*nm));
        if (!nm) {
            return;
        }
        d->msgs = nm;
        d->cap = ncap;
    }
    char *text = vs_diag_vformat(fmt, ap);
    if (!text) {
        return;
    }
    /* Keep message text in arena for lifetime simplicity */
    size_t len = strlen(text);
    char *owned = vs_arena_alloc(d->arena, len + 1);
    if (owned) {
        memcpy(owned, text, len + 1);
    }
    free(text);
    if (!owned) {
        return;
    }
    d->msgs[d->count].loc = loc;
    d->msgs[d->count].sev = sev;
    d->msgs[d->count].text = owned;
    d->count++;
    if (sev == VS_SEV_ERROR) {
        d->errors++;
    }
}

void vs_diag_error(vs_diag_t *d, vs_loc_t loc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vs_diag_push(d, VS_SEV_ERROR, loc, fmt, ap);
    va_end(ap);
}

void vs_diag_warn(vs_diag_t *d, vs_loc_t loc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vs_diag_push(d, VS_SEV_WARN, loc, fmt, ap);
    va_end(ap);
}

unsigned vs_diag_error_count(const vs_diag_t *d) {
    return d ? d->errors : 0;
}

void vs_diag_print_all(const vs_diag_t *d, FILE *out) {
    if (!d || !out) {
        return;
    }
    for (size_t i = 0; i < d->count; i++) {
        const vs_diag_msg_t *m = &d->msgs[i];
        const char *path = m->loc.path ? m->loc.path : "<unknown>";
        const char *sev = m->sev == VS_SEV_ERROR ? "error" : "warning";
        int line = m->loc.first_line > 0 ? m->loc.first_line : 1;
        int col = m->loc.first_column > 0 ? m->loc.first_column : 1;
        fprintf(out, "%s:%d:%d: %s: %s\n", path, line, col, sev, m->text);
    }
}
