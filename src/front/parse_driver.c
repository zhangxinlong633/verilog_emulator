#include "vs/parse.h"

#include "parse.tab.h"

#include <stdio.h>
#include <string.h>

typedef void *yyscan_t;

int vs_lex_init(yyscan_t *scanner);
int vs_lex_destroy(yyscan_t scanner);
void vs_set_extra(void *user_defined, yyscan_t scanner);
void vs_set_in(FILE *in_str, yyscan_t scanner);
int vs_parse(yyscan_t yyscanner, vs_parse_ctx_t *ctx);
int vs_lex(VS_STYPE *yylval, VS_LTYPE *yylloc, yyscan_t yyscanner);

static int parse_one_file(vs_parse_ctx_t *ctx, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        vs_loc_t loc = {.path = path, .first_line = 1, .first_column = 1};
        vs_diag_error(ctx->diag, loc, "cannot open file");
        return 1;
    }

    yyscan_t scanner = NULL;
    if (vs_lex_init(&scanner) != 0) {
        fclose(fp);
        vs_loc_t loc = {.path = path, .first_line = 1, .first_column = 1};
        vs_diag_error(ctx->diag, loc, "failed to init lexer");
        return 1;
    }

    ctx->filename = path;
    ctx->column = 1;
    vs_set_extra(ctx, scanner);
    vs_set_in(fp, scanner);

    int rc = vs_parse(scanner, ctx);
    vs_lex_destroy(scanner);
    fclose(fp);
    return rc != 0 || vs_diag_error_count(ctx->diag) > 0;
}

int vs_parse_files(vs_arena_t *arena, vs_strtab_t *strtab, vs_diag_t *diag, int npaths,
                   char **paths, vs_design_t **out_design) {
    if (!arena || !strtab || !diag || !out_design || npaths <= 0) {
        return 1;
    }

    vs_design_t *design = vs_design_new(arena);
    vs_parse_ctx_t ctx = {
        .arena = arena,
        .strtab = strtab,
        .diag = diag,
        .design = design,
        .filename = NULL,
        .column = 1,
    };

    int failed = 0;
    for (int i = 0; i < npaths; i++) {
        if (parse_one_file(&ctx, paths[i])) {
            failed = 1;
        }
    }

    *out_design = design;
    return failed || vs_diag_error_count(diag) > 0;
}

int vs_dump_tokens_file(FILE *out, vs_arena_t *arena, vs_strtab_t *strtab, vs_diag_t *diag,
                        const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        vs_loc_t loc = {.path = path, .first_line = 1, .first_column = 1};
        vs_diag_error(diag, loc, "cannot open file");
        return 1;
    }

    vs_parse_ctx_t ctx = {
        .arena = arena,
        .strtab = strtab,
        .diag = diag,
        .design = NULL,
        .filename = path,
        .column = 1,
    };

    yyscan_t scanner = NULL;
    if (vs_lex_init(&scanner) != 0) {
        fclose(fp);
        return 1;
    }
    vs_set_extra(&ctx, scanner);
    vs_set_in(fp, scanner);

    VS_STYPE lval;
    VS_LTYPE lloc;
    memset(&lval, 0, sizeof lval);
    memset(&lloc, 0, sizeof lloc);

    int tok;
    while ((tok = vs_lex(&lval, &lloc, scanner)) != 0) {
        fprintf(out, "%d", tok);
        if (tok == IDENT || tok == NUMBER) {
            fprintf(out, " %s", lval.str ? lval.str : "");
        }
        fprintf(out, " @%d:%d\n", lloc.first_line, lloc.first_column);
    }

    vs_lex_destroy(scanner);
    fclose(fp);
    return vs_diag_error_count(diag) > 0;
}
