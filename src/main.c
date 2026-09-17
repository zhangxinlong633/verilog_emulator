#include <stdio.h>
#include <string.h>

static void usage(FILE *out) {
    fprintf(out,
            "Usage: vs [options] <file.v> [<file.v> ...]\n"
            "Options:\n"
            "  --dump-ast       Print AST to stdout\n"
            "  --dump-tokens    Print token stream to stdout\n"
            "  -h, --help       Show this help\n"
            "  -V, --version    Show version\n");
}

int main(int argc, char **argv) {
    int saw_file = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(stdout);
            return 0;
        }
        if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
            puts("vs 0.0.0-p0");
            return 0;
        }
        if (!strcmp(argv[i], "--dump-ast") || !strcmp(argv[i], "--dump-tokens")) {
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "vs: unknown option %s\n", argv[i]);
            usage(stderr);
            return 2;
        }
        saw_file = 1;
    }

    if (!saw_file) {
        usage(stderr);
        return 2;
    }

    fprintf(stderr, "vs: parsing not implemented yet\n");
    return 2;
}
