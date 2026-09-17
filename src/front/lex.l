%{
#include "vs/parse.h"
#include "parse.tab.h"

/* Bison api.prefix {vs_} renames these; Flex still expects YY*. */
#ifndef YYSTYPE
#define YYSTYPE VS_STYPE
#endif
#ifndef YYLTYPE
#define YYLTYPE VS_LTYPE
#endif

#include <stdio.h>
#include <string.h>

#define YY_USER_ACTION                                                         \
    {                                                                          \
        vs_parse_ctx_t *_ctx = (vs_parse_ctx_t *)yyget_extra(yyscanner);       \
        yylloc->first_line = yylineno;                                         \
        yylloc->first_column = _ctx->column;                                   \
        yylloc->last_line = yylineno;                                          \
        yylloc->last_column = _ctx->column + yyleng - 1;                       \
        _ctx->column += yyleng;                                                \
    }
%}

%option noyywrap nounput noinput
%option reentrant bison-bridge bison-locations
%option yylineno
%option prefix="vs_"

%x COMMENT

%%

[ \t\r]+                  ;
\n                        { ((vs_parse_ctx_t *)yyget_extra(yyscanner))->column = 1; }

"//"[^\n]*                ;
"/*"                      { BEGIN(COMMENT); }
<COMMENT>"*/"             { BEGIN(INITIAL); }
<COMMENT>\n               { ((vs_parse_ctx_t *)yyget_extra(yyscanner))->column = 1; }
<COMMENT>.                ;
<COMMENT><<EOF>>          {
    vs_parse_ctx_t *ctx = (vs_parse_ctx_t *)yyget_extra(yyscanner);
    vs_loc_t loc = {.path = ctx->filename,
                    .first_line = yylloc->first_line,
                    .first_column = yylloc->first_column,
                    .last_line = yylloc->last_line,
                    .last_column = yylloc->last_column};
    vs_diag_error(ctx->diag, loc, "unterminated block comment");
    BEGIN(INITIAL);
    return 0;
}

"module"                  { return K_MODULE; }
"endmodule"               { return K_ENDMODULE; }
"input"                   { return K_INPUT; }
"output"                  { return K_OUTPUT; }
"inout"                   { return K_INOUT; }
"wire"                    { return K_WIRE; }
"reg"                     { return K_REG; }
"assign"                  { return K_ASSIGN; }
"always"                  { return K_ALWAYS; }
"initial"                 { return K_INITIAL; }
"begin"                   { return K_BEGIN; }
"end"                     { return K_END; }
"if"                      { return K_IF; }
"else"                    { return K_ELSE; }
"posedge"                 { return K_POSEDGE; }
"negedge"                 { return K_NEGEDGE; }
"or"                      { return K_OR; }

[0-9]+\'[sS]?[bBhHoOdD][0-9a-fA-FxXzZ_]+ {
    vs_parse_ctx_t *ctx = (vs_parse_ctx_t *)yyget_extra(yyscanner);
    size_t n = (size_t)yyleng;
    char *buf = vs_arena_alloc(ctx->arena, n + 1);
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        if (yytext[i] != '_') buf[j++] = yytext[i];
    }
    buf[j] = '\0';
    yylval->str = vs_strtab_intern(ctx->strtab, buf);
    return NUMBER;
}

[0-9][0-9_]* {
    vs_parse_ctx_t *ctx = (vs_parse_ctx_t *)yyget_extra(yyscanner);
    size_t n = (size_t)yyleng;
    char *buf = vs_arena_alloc(ctx->arena, n + 1);
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        if (yytext[i] != '_') buf[j++] = yytext[i];
    }
    buf[j] = '\0';
    yylval->str = vs_strtab_intern(ctx->strtab, buf);
    return NUMBER;
}

[a-zA-Z_][a-zA-Z0-9_$]* {
    vs_parse_ctx_t *ctx = (vs_parse_ctx_t *)yyget_extra(yyscanner);
    yylval->str = vs_strtab_intern_n(ctx->strtab, yytext, (size_t)yyleng);
    return IDENT;
}

"<="                      { return LTEQ; }
">="                      { return GTEQ; }
"=="                      { return EQEQ; }
"!="                      { return NEQ; }
"&&"                      { return LAND; }
"||"                      { return LOR; }
"~&"                      { return NAND; }
"~|"                      { return NOR; }
"~^"|"^~"                 { return XNOR; }

"{"                       { return '{'; }
"}"                       { return '}'; }
"["                       { return '['; }
"]"                       { return ']'; }
"("                       { return '('; }
")"                       { return ')'; }
","                       { return ','; }
";"                       { return ';'; }
":"                       { return ':'; }
"."                       { return '.'; }
"@"                       { return '@'; }
"*"                       { return '*'; }
"="                       { return '='; }
"+"                       { return '+'; }
"-"                       { return '-'; }
"/"                       { return '/'; }
"%"                       { return '%'; }
"&"                       { return '&'; }
"|"                       { return '|'; }
"^"                       { return '^'; }
"~"                       { return '~'; }
"!"                       { return '!'; }
"<"                       { return '<'; }
">"                       { return '>'; }

. {
    vs_parse_ctx_t *ctx = (vs_parse_ctx_t *)yyget_extra(yyscanner);
    vs_loc_t loc = {.path = ctx->filename,
                    .first_line = yylloc->first_line,
                    .first_column = yylloc->first_column,
                    .last_line = yylloc->last_line,
                    .last_column = yylloc->last_column};
    vs_diag_error(ctx->diag, loc, "unexpected character '%c'", *yytext);
}

%%
