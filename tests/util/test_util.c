#include "vs/arena.h"
#include "vs/diag.h"
#include "vs/strtab.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    vs_arena_t *a = vs_arena_create();
    assert(a);
    char *p = vs_arena_alloc(a, 16);
    assert(p && p[0] == 0);
    strcpy(p, "hi");

    vs_strtab_t *t = vs_strtab_create(a);
    const char *s1 = vs_strtab_intern(t, "clk");
    const char *s2 = vs_strtab_intern(t, "clk");
    assert(s1 == s2);

    vs_diag_t *d = vs_diag_create(a);
    vs_loc_t loc = {.path = "t.v",
                    .first_line = 3,
                    .first_column = 5,
                    .last_line = 3,
                    .last_column = 5};
    vs_diag_error(d, loc, "syntax error");
    assert(vs_diag_error_count(d) == 1);
    vs_diag_print_all(d, stdout);

    vs_arena_destroy(a);
    puts("ok");
    return 0;
}
