#include "vs/arena.h"
#include "vs/ast.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    vs_arena_t *a = vs_arena_create();
    vs_design_t *d = vs_design_new(a);
    vs_loc_t loc = {0};
    vs_module_t *m = vs_module_new(a, loc, "empty");
    vs_design_add_module(d, m);

    FILE *tmp = tmpfile();
    if (!tmp) {
        vs_arena_destroy(a);
        return 1;
    }
    vs_dump_ast(tmp, d);
    rewind(tmp);

    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';
    fclose(tmp);

    if (strcmp(buf, "design\n  module empty\n") != 0) {
        fprintf(stderr, "bad dump:\n[%s]\n", buf);
        vs_arena_destroy(a);
        return 1;
    }

    vs_arena_destroy(a);
    return 0;
}
