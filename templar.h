#pragma once
#include <stdint.h>
#include <stddef.h>

#define TEMPLAR_MAX_VARS      64
#define TEMPLAR_MAX_FUNCS     32
#define TEMPLAR_MAX_CALLS     64
#define TEMPLAR_MAX_STR       256
#define TEMPLAR_MAX_SRC       8192
#define TEMPLAR_MAX_TOKENS    512
#define TEMPLAR_MAX_LINES     256
#define TEMPLAR_STACK_SIZE    32

typedef enum {
    VAL_NUM = 0,
    VAL_STR = 1,
    VAL_NIL = 2,
} TplType;

typedef struct {
    TplType type;
    int32_t num;
    char    str[TEMPLAR_MAX_STR];
} TplVal;

typedef struct {
    char   name[64];
    TplVal val;
} TplVar;

typedef struct {
    char   name[64];
    int    line;
    int    param_count;
    char   params[8][64];
} TplFunc;

typedef enum {
    TK_NAME = 0,
    TK_NUM,
    TK_STR,
    TK_EQ,
    TK_EQEQ,
    TK_NEQ,
    TK_LT,
    TK_GT,
    TK_LTE,
    TK_GTE,
    TK_PLUS,
    TK_MINUS,
    TK_STAR,
    TK_SLASH,
    TK_LPAREN,
    TK_RPAREN,
    TK_LBRACE,
    TK_RBRACE,
    TK_COLON,
    TK_COMMA,
    TK_NEWLINE,
    TK_EOF,
    TK_AND,
    TK_OR,
    TK_NOT,
    TK_PERCENT,
} TplTokKind;

typedef struct {
    TplTokKind kind;
    char       text[TEMPLAR_MAX_STR];
    int32_t    num;
    int        line;
} TplTok;

typedef struct {
    char      src[TEMPLAR_MAX_SRC];
    int       src_len;
    TplTok    tokens[TEMPLAR_MAX_TOKENS];
    int       tok_count;
    int       tok_pos;
    TplVar    vars[TEMPLAR_MAX_VARS];
    int       var_count;
    TplFunc   funcs[TEMPLAR_MAX_FUNCS];
    int       func_count;
    TplVal    call_stack[TEMPLAR_STACK_SIZE];
    int       call_depth;
    int       error;
    char      error_msg[128];
} TplState;

void    templar_init(TplState *s);
int     templar_load_file(TplState *s, const char *filename);
int     templar_run(TplState *s);
int     templar_run_string(TplState *s, const char *code);
void    templar_set_var(TplState *s, const char *name, TplVal val);
TplVal  templar_get_var(TplState *s, const char *name);
TplVal  tpl_num(int32_t n);
TplVal  tpl_str(const char *s);
TplVal  tpl_nil(void);
