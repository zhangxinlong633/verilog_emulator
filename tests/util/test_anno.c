#include "vs/anno.h"
#include "vs/arena.h"
#include "vs/diag.h"

#include <stdio.h>
#include <string.h>

static int expect(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        return 1;
    }
    return 0;
}

int main(void) {
    const char *path = "tests/util/fixtures/anno_matmul.v";
    vs_arena_t *a = vs_arena_create();
    vs_diag_t *d = vs_diag_create(a);
    int fail = 0;

    fail |= expect(a != NULL && d != NULL, "arena/diag");
    vs_anno_set_t *set = vs_anno_scan_file(a, d, path);
    fail |= expect(set != NULL, "set non-null");
    fail |= expect(set && set->nmatrices == 3, "3 matrices");
    fail |= expect(set && set->nops == 1, "1 op");
    fail |= expect(set && set->nexprs == 4, "4 exprs");
    if (set && set->nmatrices >= 1) {
        fail |= expect(strcmp(set->matrices[0].name, "A") == 0, "A name");
        fail |= expect(set->matrices[0].rows == 2 && set->matrices[0].cols == 2, "A shape");
        fail |= expect(strcmp(set->matrices[0].cells[0][0], "a00") == 0, "A[0][0]");
        fail |= expect(strcmp(set->matrices[0].cells[1][1], "a11") == 0, "A[1][1]");
    }
    if (set && set->nops >= 1) {
        fail |= expect(strcmp(set->ops[0].kind, "matmul") == 0, "op kind");
        fail |= expect(strcmp(set->ops[0].out, "C") == 0, "op out");
        fail |= expect(strcmp(set->ops[0].left, "A") == 0, "op left");
        fail |= expect(strcmp(set->ops[0].right, "B") == 0, "op right");
    }
    fail |= expect(set && vs_anno_find_expr(set, "c00") != NULL, "c00 expr");
    fail |= expect(set && strstr(vs_anno_find_expr(set, "c00"), "a00") != NULL, "c00 has a00");

    vs_arena_destroy(a);
    if (fail) {
        return 1;
    }
    puts("ok");
    return 0;
}
