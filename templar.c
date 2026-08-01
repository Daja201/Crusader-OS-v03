#include "templar.h"
#include "vesa.h"
#include "klog.h"
#include "fs.h"
#include "string.h"
#include <stdint.h>
#include <stddef.h>

extern int32_t  c_x;
extern int32_t  c_y;
extern uint32_t g_current_dir;
extern void     read_inode(int idx, inode_t *inode);
extern int      fs_resolve_path(const char *path, uint32_t current_dir_inode);

static int tpl_isalpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int tpl_isdigit(char c) { return c >= '0' && c <= '9'; }
static int tpl_isalnum(char c) { return tpl_isalpha(c) || tpl_isdigit(c); }
static int tpl_isspace(char c) { return c == ' ' || c == '\t' || c == '\r'; }

static int tpl_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
static void tpl_strcpy(char *d, const char *s) {
    while ((*d++ = *s++));
}
static int tpl_strlen(const char *s) {
    int n = 0; while (s[n]) n++; return n;
}
static void tpl_strcat(char *d, const char *s) {
    while (*d) d++;
    while ((*d++ = *s++));
}
static void tpl_memset(void *p, int v, int n) {
    uint8_t *b = (uint8_t*)p;
    for (int i = 0; i < n; i++) b[i] = (uint8_t)v;
}

static void tpl_itoa(int32_t v, char *buf) {
    if (v == 0) { buf[0]='0'; buf[1]=0; return; }
    char tmp[16]; int i=0, neg=0;
    if (v < 0) { neg=1; v=-v; }
    while (v > 0) { tmp[i++] = '0' + (v%10); v/=10; }
    if (neg) tmp[i++]='-';
    for (int j=0;j<i;j++) buf[j]=tmp[i-1-j];
    buf[i]=0;
}
static int32_t tpl_atoi(const char *s) {
    int32_t v=0, neg=0;
    if (*s=='-'){neg=1;s++;}
    while (tpl_isdigit(*s)) v=v*10+(*s++)-'0';
    return neg?-v:v;
}

TplVal tpl_num(int32_t n) { TplVal v; v.type=VAL_NUM; v.num=n; v.str[0]=0; return v; }
TplVal tpl_str(const char *s) { TplVal v; v.type=VAL_STR; v.num=0; tpl_strcpy(v.str,s); return v; }
TplVal tpl_nil(void) { TplVal v; v.type=VAL_NIL; v.num=0; v.str[0]=0; return v; }

static void tpl_val_to_str(TplVal *v, char *buf) {
    if (v->type == VAL_NUM) tpl_itoa(v->num, buf);
    else if (v->type == VAL_STR) tpl_strcpy(buf, v->str);
    else tpl_strcpy(buf, "nil");
}

static int tpl_val_truthy(TplVal *v) {
    if (v->type == VAL_NIL) return 0;
    if (v->type == VAL_NUM) return v->num != 0;
    if (v->type == VAL_STR) return v->str[0] != 0;
    return 0;
}

static int32_t tpl_val_num(TplVal *v) {
    if (v->type == VAL_NUM) return v->num;
    if (v->type == VAL_STR) return tpl_atoi(v->str);
    return 0;
}

void templar_init(TplState *s) {
    tpl_memset(s, 0, sizeof(TplState));
}

void templar_set_var(TplState *s, const char *name, TplVal val) {
    for (int i = 0; i < s->var_count; i++) {
        if (tpl_strcmp(s->vars[i].name, name) == 0) {
            s->vars[i].val = val;
            return;
        }
    }
    if (s->var_count < TEMPLAR_MAX_VARS) {
        tpl_strcpy(s->vars[s->var_count].name, name);
        s->vars[s->var_count].val = val;
        s->var_count++;
    }
}

TplVal templar_get_var(TplState *s, const char *name) {
    for (int i = 0; i < s->var_count; i++) {
        if (tpl_strcmp(s->vars[i].name, name) == 0)
            return s->vars[i].val;
    }
    return tpl_nil();
}

static void tpl_error(TplState *s, const char *msg) {
    s->error = 1;
    tpl_strcpy(s->error_msg, msg);
}

static void tpl_lexer(TplState *s) {
    const char *p = s->src;
    int line = 1;
    s->tok_count = 0;

    while (*p && s->tok_count < TEMPLAR_MAX_TOKENS - 1) {
        while (tpl_isspace(*p)) p++;

        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        if (*p == '\n') {
            TplTok *t = &s->tokens[s->tok_count++];
            t->kind = TK_NEWLINE; t->line = line; t->text[0]='\n'; t->text[1]=0;
            line++; p++; continue;
        }

        if (*p == 0) break;

        TplTok *t = &s->tokens[s->tok_count];
        t->line = line;

        if (tpl_isalpha(*p)) {
            int i = 0;
            while (tpl_isalnum(*p) && i < TEMPLAR_MAX_STR-1)
                t->text[i++] = *p++;
            t->text[i] = 0;
            t->kind = TK_NAME;
            if (tpl_strcmp(t->text,"and")==0) t->kind=TK_AND;
            else if (tpl_strcmp(t->text,"or")==0) t->kind=TK_OR;
            else if (tpl_strcmp(t->text,"not")==0) t->kind=TK_NOT;
            s->tok_count++; continue;
        }

        if (tpl_isdigit(*p) || (*p=='-' && tpl_isdigit(*(p+1)))) {
            int i = 0; int neg = 0;
            if (*p=='-') { neg=1; t->text[i++]=*p++; }
            while (tpl_isdigit(*p) && i < 15) t->text[i++]=*p++;
            t->text[i]=0;
            t->kind = TK_NUM;
            t->num  = tpl_atoi(t->text);
            s->tok_count++; continue;
        }

        if (*p == '"') {
            p++;
            int i = 0;
            while (*p && *p != '"' && i < TEMPLAR_MAX_STR-1) {
                if (*p == '\\') {
                    p++;
                    if (*p == 'n') t->text[i++]='\n';
                    else if (*p == 't') t->text[i++]='\t';
                    else t->text[i++]=*p;
                    p++;
                } else {
                    t->text[i++] = *p++;
                }
            }
            if (*p == '"') p++;
            t->text[i]=0; t->kind=TK_STR; t->num=0;
            s->tok_count++; continue;
        }

        if (*p=='=' && *(p+1)=='=') { t->kind=TK_EQEQ; tpl_strcpy(t->text,"=="); p+=2; s->tok_count++; continue; }
        if (*p=='!' && *(p+1)=='=') { t->kind=TK_NEQ;  tpl_strcpy(t->text,"!="); p+=2; s->tok_count++; continue; }
        if (*p=='<' && *(p+1)=='=') { t->kind=TK_LTE;  tpl_strcpy(t->text,"<="); p+=2; s->tok_count++; continue; }
        if (*p=='>' && *(p+1)=='=') { t->kind=TK_GTE;  tpl_strcpy(t->text,">="); p+=2; s->tok_count++; continue; }

        switch (*p) {
            case '=': t->kind=TK_EQ;     break;
            case '<': t->kind=TK_LT;     break;
            case '>': t->kind=TK_GT;     break;
            case '+': t->kind=TK_PLUS;   break;
            case '-': t->kind=TK_MINUS;  break;
            case '*': t->kind=TK_STAR;   break;
            case '/': t->kind=TK_SLASH;  break;
            case '%': t->kind=TK_PERCENT;break;
            case '(': t->kind=TK_LPAREN; break;
            case ')': t->kind=TK_RPAREN; break;
            case '{': t->kind=TK_LBRACE; break;
            case '}': t->kind=TK_RBRACE; break;
            case ':': t->kind=TK_COLON;  break;
            case ',': t->kind=TK_COMMA;  break;
            default:  p++; continue;
        }
        t->text[0]=*p; t->text[1]=0;
        s->tok_count++; p++;
    }
    s->tokens[s->tok_count].kind = TK_EOF;
    s->tokens[s->tok_count].line = line;
    s->tok_count++;
}

static TplTok *tpl_peek(TplState *s) {
    return &s->tokens[s->tok_pos];
}
static TplTok *tpl_advance(TplState *s) {
    TplTok *t = &s->tokens[s->tok_pos];
    if (t->kind != TK_EOF) s->tok_pos++;
    return t;
}
static void tpl_skip_newlines(TplState *s) {
    while (tpl_peek(s)->kind == TK_NEWLINE) tpl_advance(s);
}
static int tpl_expect(TplState *s, TplTokKind k) {
    if (tpl_peek(s)->kind == k) { tpl_advance(s); return 1; }
    tpl_error(s, "unexpected token");
    return 0;
}
static int tpl_match(TplState *s, TplTokKind k) {
    if (tpl_peek(s)->kind == k) { tpl_advance(s); return 1; }
    return 0;
}

static TplVal tpl_exec_block(TplState *s);
static TplVal tpl_exec_statement(TplState *s);
static TplVal tpl_expr(TplState *s);

static uint32_t tpl_parse_color(TplState *s) {
    TplVal v = tpl_expr(s);
    if (v.type == VAL_STR) {
        const char *cs = v.str;
        if (cs[0]=='0'&&cs[1]=='x') return (uint32_t)tpl_atoi(cs+2);
        if (cs[0]=='#') {
            uint32_t r=0,g=0,b=0;
            const char *hex = cs+1;
            for (int i=0;i<6;i++) {
                char c=hex[i]; uint8_t nib;
                if (c>='0'&&c<='9') nib=c-'0';
                else if (c>='a'&&c<='f') nib=c-'a'+10;
                else if (c>='A'&&c<='F') nib=c-'A'+10;
                else nib=0;
                if (i<2) r=(r<<4)|nib;
                else if (i<4) g=(g<<4)|nib;
                else b=(b<<4)|nib;
            }
            return (r<<16)|(g<<8)|b;
        }
        return (uint32_t)tpl_atoi(cs);
    }
    return (uint32_t)tpl_val_num(&v);
}

static TplVal tpl_call_builtin(TplState *s, const char *name) {
    tpl_expect(s, TK_LPAREN);
    if (s->error) return tpl_nil();

    if (tpl_strcmp(name,"print")==0) {
        char buf[TEMPLAR_MAX_STR]; buf[0]=0;
        while (tpl_peek(s)->kind != TK_RPAREN && tpl_peek(s)->kind != TK_EOF) {
            TplVal v = tpl_expr(s);
            char tmp[TEMPLAR_MAX_STR];
            tpl_val_to_str(&v, tmp);
            tpl_strcat(buf, tmp);
            if (tpl_peek(s)->kind==TK_COMMA) tpl_advance(s);
        }
        tpl_expect(s, TK_RPAREN);
        klog(buf); klog("\n");
        return tpl_nil();
    }

    if (tpl_strcmp(name,"print_raw")==0) {
        char buf[TEMPLAR_MAX_STR]; buf[0]=0;
        while (tpl_peek(s)->kind != TK_RPAREN && tpl_peek(s)->kind != TK_EOF) {
            TplVal v = tpl_expr(s);
            char tmp[TEMPLAR_MAX_STR];
            tpl_val_to_str(&v, tmp);
            tpl_strcat(buf, tmp);
            if (tpl_peek(s)->kind==TK_COMMA) tpl_advance(s);
        }
        tpl_expect(s, TK_RPAREN);
        klog(buf);
        return tpl_nil();
    }

    if (tpl_strcmp(name,"cls")==0) {
        TplVal cv = tpl_expr(s); tpl_expect(s,TK_RPAREN);
        uint32_t col = (uint32_t)tpl_val_num(&cv);
        vesa_clear(col);
        return tpl_nil();
    }

    if (tpl_strcmp(name,"swap")==0) {
        tpl_expect(s, TK_RPAREN);
        extern void vesa_swap(void);
        vesa_swap();
        return tpl_nil();
    }

    if (tpl_strcmp(name,"pixel")==0) {
        TplVal vx=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vy=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vc=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        vesa_putpixel(tpl_val_num(&vx), tpl_val_num(&vy), (uint32_t)tpl_val_num(&vc));
        return tpl_nil();
    }

    if (tpl_strcmp(name,"rect")==0) {
        TplVal vx=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vy=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vw=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vh=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vc=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        vesa_draw_rec(tpl_val_num(&vx),tpl_val_num(&vy),
                      tpl_val_num(&vw),tpl_val_num(&vh),(uint32_t)tpl_val_num(&vc));
        return tpl_nil();
    }

    if (tpl_strcmp(name,"line_h")==0) {
        TplVal vx=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vy=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vl=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vc=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        vesa_draw_hor(tpl_val_num(&vx),tpl_val_num(&vy),
                      tpl_val_num(&vl),(uint32_t)tpl_val_num(&vc));
        return tpl_nil();
    }

    if (tpl_strcmp(name,"line_v")==0) {
        TplVal vx=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vy=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vl=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vc=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        vesa_draw_ver(tpl_val_num(&vx),tpl_val_num(&vy),
                      tpl_val_num(&vl),(uint32_t)tpl_val_num(&vc));
        return tpl_nil();
    }

    if (tpl_strcmp(name,"text")==0) {
        TplVal vx=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vy=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vs=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vfg=tpl_expr(s);
        uint32_t bg=0x000000;
        if (tpl_peek(s)->kind==TK_COMMA) {
            tpl_advance(s);
            TplVal vbg=tpl_expr(s); bg=(uint32_t)tpl_val_num(&vbg);
        }
        tpl_expect(s,TK_RPAREN);
        char tmp[TEMPLAR_MAX_STR]; tpl_val_to_str(&vs,tmp);
        int px=tpl_val_num(&vx), py=tpl_val_num(&vy);
        uint32_t fg=(uint32_t)tpl_val_num(&vfg);
        for (int i=0;tmp[i];i++) {
            vesa_draw_char(tmp[i],px,py,fg,bg);
            px+=8;
        }
        return tpl_nil();
    }

    if (tpl_strcmp(name,"print_at")==0) {
        TplVal vx=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vy=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vs=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        char tmp[TEMPLAR_MAX_STR]; tpl_val_to_str(&vs,tmp);
        c_x=tpl_val_num(&vx); c_y=tpl_val_num(&vy);
        klog(tmp);
        return tpl_nil();
    }

    if (tpl_strcmp(name,"circle")==0) {
        TplVal vcx=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vcy=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vr=tpl_expr(s);  tpl_match(s,TK_COMMA);
        TplVal vc=tpl_expr(s);  tpl_expect(s,TK_RPAREN);
        int cx=tpl_val_num(&vcx), cy=tpl_val_num(&vcy), r=tpl_val_num(&vr);
        uint32_t col=(uint32_t)tpl_val_num(&vc);
        int x=0,y=r,d=3-2*r;
        while(x<=y){
            vesa_putpixel(cx+x,cy+y,col); vesa_putpixel(cx-x,cy+y,col);
            vesa_putpixel(cx+x,cy-y,col); vesa_putpixel(cx-x,cy-y,col);
            vesa_putpixel(cx+y,cy+x,col); vesa_putpixel(cx-y,cy+x,col);
            vesa_putpixel(cx+y,cy-x,col); vesa_putpixel(cx-y,cy-x,col);
            if(d<0) d+=4*x+6; else { d+=4*(x-y)+10; y--; } x++;
        }
        return tpl_nil();
    }

    if (tpl_strcmp(name,"filled_circle")==0) {
        TplVal vcx=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vcy=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vr=tpl_expr(s);  tpl_match(s,TK_COMMA);
        TplVal vc=tpl_expr(s);  tpl_expect(s,TK_RPAREN);
        int cx=tpl_val_num(&vcx), cy=tpl_val_num(&vcy), r=tpl_val_num(&vr);
        uint32_t col=(uint32_t)tpl_val_num(&vc);
        for(int dy=-r;dy<=r;dy++)
            for(int dx=-r;dx<=r;dx++)
                if(dx*dx+dy*dy<=r*r)
                    vesa_putpixel(cx+dx,cy+dy,col);
        return tpl_nil();
    }

    if (tpl_strcmp(name,"triangle")==0) {
        TplVal vx1=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vy1=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vx2=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vy2=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vx3=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vy3=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vc=tpl_expr(s);  tpl_expect(s,TK_RPAREN);
        int x1=tpl_val_num(&vx1),y1=tpl_val_num(&vy1);
        int x2=tpl_val_num(&vx2),y2=tpl_val_num(&vy2);
        int x3=tpl_val_num(&vx3),y3=tpl_val_num(&vy3);
        uint32_t col=(uint32_t)tpl_val_num(&vc);
        #define TPL_SWAP(a,b) {int _t=a;a=b;b=_t;}
        if(y1>y2){TPL_SWAP(x1,x2)TPL_SWAP(y1,y2)}
        if(y1>y3){TPL_SWAP(x1,x3)TPL_SWAP(y1,y3)}
        if(y2>y3){TPL_SWAP(x2,x3)TPL_SWAP(y2,y3)}
        for(int y=y1;y<=y3;y++){
            int xa,xb;
            if(y<y2){
                xa=y1!=y2? x1+(x2-x1)*(y-y1)/(y2-y1) : x1;
            } else {
                xa=y2!=y3? x2+(x3-x2)*(y-y2)/(y3-y2) : x2;
            }
            xb=y1!=y3? x1+(x3-x1)*(y-y1)/(y3-y1) : x1;
            if(xa>xb){TPL_SWAP(xa,xb)}
            vesa_draw_hor(xa,y,xb-xa+1,col);
        }
        return tpl_nil();
    }

    if (tpl_strcmp(name,"sleep")==0) {
        TplVal vm=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        volatile uint32_t iter = 8000 * (uint32_t)tpl_val_num(&vm);
        for (volatile uint32_t i=0;i<iter;i++) asm volatile("nop");
        return tpl_nil();
    }

    if (tpl_strcmp(name,"int")==0) {
        TplVal v=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        return tpl_num(tpl_val_num(&v));
    }

    if (tpl_strcmp(name,"str")==0) {
        TplVal v=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        char buf[TEMPLAR_MAX_STR]; tpl_val_to_str(&v,buf);
        return tpl_str(buf);
    }

    if (tpl_strcmp(name,"len")==0) {
        TplVal v=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        char buf[TEMPLAR_MAX_STR]; tpl_val_to_str(&v,buf);
        return tpl_num(tpl_strlen(buf));
    }

    if (tpl_strcmp(name,"abs")==0) {
        TplVal v=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        int32_t n=tpl_val_num(&v);
        return tpl_num(n<0?-n:n);
    }

    if (tpl_strcmp(name,"min")==0) {
        TplVal a=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal b=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        int32_t an=tpl_val_num(&a), bn=tpl_val_num(&b);
        return tpl_num(an<bn?an:bn);
    }

    if (tpl_strcmp(name,"max")==0) {
        TplVal a=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal b=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        int32_t an=tpl_val_num(&a), bn=tpl_val_num(&b);
        return tpl_num(an>bn?an:bn);
    }

    if (tpl_strcmp(name,"concat")==0) {
        char buf[TEMPLAR_MAX_STR]; buf[0]=0;
        while (tpl_peek(s)->kind!=TK_RPAREN&&tpl_peek(s)->kind!=TK_EOF){
            TplVal v=tpl_expr(s);
            char tmp[TEMPLAR_MAX_STR]; tpl_val_to_str(&v,tmp);
            tpl_strcat(buf,tmp);
            if(tpl_peek(s)->kind==TK_COMMA) tpl_advance(s);
        }
        tpl_expect(s,TK_RPAREN);
        return tpl_str(buf);
    }

    if (tpl_strcmp(name,"numstr")==0) {
        TplVal v=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        char buf[32]; tpl_itoa(tpl_val_num(&v),buf);
        return tpl_str(buf);
    }

    if (tpl_strcmp(name,"screen_w")==0) {
        tpl_expect(s,TK_RPAREN);
        return tpl_num(1280);
    }
    if (tpl_strcmp(name,"screen_h")==0) {
        tpl_expect(s,TK_RPAREN);
        return tpl_num(720);
    }
    if (tpl_strcmp(name,"cursor_x")==0) {
        tpl_expect(s,TK_RPAREN);
        return tpl_num(c_x);
    }
    if (tpl_strcmp(name,"cursor_y")==0) {
        tpl_expect(s,TK_RPAREN);
        return tpl_num(c_y);
    }
    if (tpl_strcmp(name,"set_cursor")==0) {
        TplVal vx=tpl_expr(s); tpl_match(s,TK_COMMA);
        TplVal vy=tpl_expr(s); tpl_expect(s,TK_RPAREN);
        c_x=tpl_val_num(&vx); c_y=tpl_val_num(&vy);
        return tpl_nil();
    }

    tpl_error(s, "unknown builtin");
    while(tpl_peek(s)->kind!=TK_RPAREN&&tpl_peek(s)->kind!=TK_EOF) tpl_advance(s);
    tpl_match(s,TK_RPAREN);
    return tpl_nil();
}

static TplVal tpl_primary(TplState *s) {
    if (s->error) return tpl_nil();
    TplTok *t = tpl_peek(s);

    if (t->kind == TK_NUM) {
        tpl_advance(s);
        return tpl_num(t->num);
    }
    if (t->kind == TK_STR) {
        tpl_advance(s);
        return tpl_str(t->text);
    }
    if (t->kind == TK_NOT) {
        tpl_advance(s);
        TplVal v = tpl_primary(s);
        return tpl_num(!tpl_val_truthy(&v));
    }
    if (t->kind == TK_MINUS) {
        tpl_advance(s);
        TplVal v = tpl_primary(s);
        return tpl_num(-tpl_val_num(&v));
    }
    if (t->kind == TK_LPAREN) {
        tpl_advance(s);
        TplVal v = tpl_expr(s);
        tpl_expect(s, TK_RPAREN);
        return v;
    }
    if (t->kind == TK_NAME) {
        char name[64];
        tpl_strcpy(name, t->text);
        tpl_advance(s);
        if (tpl_peek(s)->kind == TK_LPAREN) {
            return tpl_call_builtin(s, name);
        }
        return templar_get_var(s, name);
    }
    tpl_advance(s);
    return tpl_nil();
}

static TplVal tpl_term(TplState *s) {
    TplVal left = tpl_primary(s);
    while (!s->error) {
        TplTokKind k = tpl_peek(s)->kind;
        if (k!=TK_STAR && k!=TK_SLASH && k!=TK_PERCENT) break;
        tpl_advance(s);
        TplVal right = tpl_primary(s);
        int32_t a=tpl_val_num(&left), b=tpl_val_num(&right);
        if (k==TK_STAR) left=tpl_num(a*b);
        else if (k==TK_SLASH) left=tpl_num(b?a/b:0);
        else left=tpl_num(b?a%b:0);
    }
    return left;
}

static TplVal tpl_additive(TplState *s) {
    TplVal left = tpl_term(s);
    while (!s->error) {
        TplTokKind k = tpl_peek(s)->kind;
        if (k!=TK_PLUS && k!=TK_MINUS) break;
        tpl_advance(s);
        TplVal right = tpl_term(s);
        if (k==TK_PLUS) {
            if (left.type==VAL_STR || right.type==VAL_STR) {
                char buf[TEMPLAR_MAX_STR]; buf[0]=0;
                char tmp[TEMPLAR_MAX_STR];
                tpl_val_to_str(&left,buf);
                tpl_val_to_str(&right,tmp);
                tpl_strcat(buf,tmp);
                left=tpl_str(buf);
            } else {
                left=tpl_num(tpl_val_num(&left)+tpl_val_num(&right));
            }
        } else {
            left=tpl_num(tpl_val_num(&left)-tpl_val_num(&right));
        }
    }
    return left;
}

static TplVal tpl_comparison(TplState *s) {
    TplVal left = tpl_additive(s);
    while (!s->error) {
        TplTokKind k = tpl_peek(s)->kind;
        if (k!=TK_EQEQ&&k!=TK_NEQ&&k!=TK_LT&&k!=TK_GT&&k!=TK_LTE&&k!=TK_GTE) break;
        tpl_advance(s);
        TplVal right = tpl_additive(s);
        int result;
        if (left.type==VAL_STR || right.type==VAL_STR) {
            char la[TEMPLAR_MAX_STR], ra[TEMPLAR_MAX_STR];
            tpl_val_to_str(&left,la); tpl_val_to_str(&right,ra);
            int c = tpl_strcmp(la,ra);
            if(k==TK_EQEQ) result=(c==0);
            else if(k==TK_NEQ) result=(c!=0);
            else if(k==TK_LT) result=(c<0);
            else if(k==TK_GT) result=(c>0);
            else if(k==TK_LTE) result=(c<=0);
            else result=(c>=0);
        } else {
            int32_t a=tpl_val_num(&left), b=tpl_val_num(&right);
            if(k==TK_EQEQ) result=(a==b);
            else if(k==TK_NEQ) result=(a!=b);
            else if(k==TK_LT) result=(a<b);
            else if(k==TK_GT) result=(a>b);
            else if(k==TK_LTE) result=(a<=b);
            else result=(a>=b);
        }
        left=tpl_num(result);
    }
    return left;
}

static TplVal tpl_expr(TplState *s) {
    TplVal left = tpl_comparison(s);
    while (!s->error) {
        TplTokKind k = tpl_peek(s)->kind;
        if (k!=TK_AND && k!=TK_OR) break;
        tpl_advance(s);
        TplVal right = tpl_comparison(s);
        int a=tpl_val_truthy(&left), b=tpl_val_truthy(&right);
        left=tpl_num(k==TK_AND ? (a&&b) : (a||b));
    }
    return left;
}

#define TPL_RET_RETURN  1
#define TPL_RET_BREAK   2
#define TPL_RET_CONT    3
#define TPL_RET_NONE    0

typedef struct { int flag; TplVal val; } TplExec;
static TplExec tpl_exec_block_ex(TplState *s);
static TplExec tpl_exec_stmt_ex(TplState *s);

static TplExec tpl_exec_stmt_ex(TplState *s) {
    TplExec result; result.flag=TPL_RET_NONE; result.val=tpl_nil();
    if (s->error) return result;
    tpl_skip_newlines(s);

    TplTok *t = tpl_peek(s);
    if (t->kind == TK_EOF) return result;

    if (t->kind==TK_NAME && tpl_strcmp(t->text,"var")==0) {
        tpl_advance(s);
        TplTok *name_tok = tpl_peek(s);
        if (name_tok->kind!=TK_NAME) { tpl_error(s,"expected var name"); return result; }
        char vname[64]; tpl_strcpy(vname, name_tok->text);
        tpl_advance(s);
        TplVal val = tpl_nil();
        if (tpl_peek(s)->kind==TK_EQ) {
            tpl_advance(s);
            val = tpl_expr(s);
        }
        templar_set_var(s, vname, val);
        return result;
    }

    if (t->kind==TK_NAME && tpl_peek(s+0)->kind==TK_NAME) {
        TplTok *next = &s->tokens[s->tok_pos+1];
        if (next->kind==TK_EQ) {
            char vname[64]; tpl_strcpy(vname, t->text);
            tpl_advance(s); tpl_advance(s);
            TplVal val = tpl_expr(s);
            templar_set_var(s, vname, val);
            return result;
        }
    }
    if (t->kind==TK_NAME) {
        TplTok *next = &s->tokens[s->tok_pos+1];
        if (next->kind==TK_EQ) {
            char vname[64]; tpl_strcpy(vname, t->text);
            tpl_advance(s); tpl_advance(s);
            TplVal val = tpl_expr(s);
            templar_set_var(s, vname, val);
            return result;
        }
    }

    if (t->kind==TK_NAME && tpl_strcmp(t->text,"if")==0) {
        tpl_advance(s);
        TplVal cond = tpl_expr(s);
        tpl_skip_newlines(s);
        tpl_expect(s, TK_COLON);
        tpl_skip_newlines(s);
        tpl_expect(s, TK_LBRACE);
        if (tpl_val_truthy(&cond)) {
            TplExec r = tpl_exec_block_ex(s);
            tpl_expect(s,TK_RBRACE);
            tpl_skip_newlines(s);
            if (tpl_peek(s)->kind==TK_NAME && tpl_strcmp(tpl_peek(s)->text,"else")==0) {
                tpl_advance(s);
                tpl_skip_newlines(s);
                tpl_expect(s,TK_COLON);
                tpl_skip_newlines(s);
                tpl_expect(s,TK_LBRACE);
                int depth=1;
                while (depth>0 && tpl_peek(s)->kind!=TK_EOF) {
                    if (tpl_peek(s)->kind==TK_LBRACE) depth++;
                    else if(tpl_peek(s)->kind==TK_RBRACE) depth--;
                    if(depth>0) tpl_advance(s); else tpl_advance(s);
                }
            }
            return r;
        } else {
            int depth=1;
            while (depth>0 && tpl_peek(s)->kind!=TK_EOF) {
                if (tpl_peek(s)->kind==TK_LBRACE) depth++;
                else if (tpl_peek(s)->kind==TK_RBRACE) depth--;
                if (depth>0) tpl_advance(s); else tpl_advance(s);
            }
            tpl_skip_newlines(s);
            if (tpl_peek(s)->kind==TK_NAME && tpl_strcmp(tpl_peek(s)->text,"else")==0) {
                tpl_advance(s);
                tpl_skip_newlines(s);
                tpl_expect(s,TK_COLON);
                tpl_skip_newlines(s);
                tpl_expect(s,TK_LBRACE);
                TplExec r = tpl_exec_block_ex(s);
                tpl_expect(s,TK_RBRACE);
                return r;
            }
            return result;
        }
    }

    if (t->kind==TK_NAME && tpl_strcmp(t->text,"while")==0) {
        tpl_advance(s);
        int cond_pos = s->tok_pos;
        for (;;) {
            s->tok_pos = cond_pos;
            TplVal cond = tpl_expr(s);
            tpl_skip_newlines(s);
            tpl_expect(s,TK_COLON);
            tpl_skip_newlines(s);
            tpl_expect(s,TK_LBRACE);
            if (!tpl_val_truthy(&cond)) {
                int depth=1;
                while(depth>0&&tpl_peek(s)->kind!=TK_EOF){
                    if(tpl_peek(s)->kind==TK_LBRACE) depth++;
                    else if(tpl_peek(s)->kind==TK_RBRACE) depth--;
                    tpl_advance(s);
                }
                break;
            }
            TplExec r = tpl_exec_block_ex(s);
            tpl_expect(s,TK_RBRACE);
            if (r.flag==TPL_RET_BREAK) break;
            if (r.flag==TPL_RET_RETURN) return r;
            if (s->error) break;
        }
        return result;
    }

    if (t->kind==TK_NAME && tpl_strcmp(t->text,"repeat")==0) {
        tpl_advance(s);
        TplVal cnt_v = tpl_expr(s);
        int32_t cnt = tpl_val_num(&cnt_v);
        tpl_skip_newlines(s);
        tpl_expect(s,TK_COLON);
        tpl_skip_newlines(s);
        tpl_expect(s,TK_LBRACE);
        int body_start = s->tok_pos;
        for (int32_t i=0;i<cnt;i++) {
            s->tok_pos = body_start;
            TplExec r = tpl_exec_block_ex(s);
            if (r.flag==TPL_RET_BREAK) { s->tok_pos=body_start; tpl_exec_block_ex(s); break; }
            if (r.flag==TPL_RET_RETURN) {
                s->tok_pos=body_start;
                int depth=1;
                while(depth>0&&tpl_peek(s)->kind!=TK_EOF){
                    if(tpl_peek(s)->kind==TK_LBRACE)depth++;
                    else if(tpl_peek(s)->kind==TK_RBRACE)depth--;
                    tpl_advance(s);
                }
                return r;
            }
            if (s->error) break;
        }
        if (cnt<=0||s->error) {
            int depth=1;
            while(depth>0&&tpl_peek(s)->kind!=TK_EOF){
                if(tpl_peek(s)->kind==TK_LBRACE)depth++;
                else if(tpl_peek(s)->kind==TK_RBRACE)depth--;
                tpl_advance(s);
            }
        } else {
            tpl_expect(s,TK_RBRACE);
        }
        return result;
    }

    if (t->kind==TK_NAME && tpl_strcmp(t->text,"for")==0) {
        tpl_advance(s);
        TplTok *vt = tpl_peek(s);
        if (vt->kind!=TK_NAME){tpl_error(s,"for: expected var name");return result;}
        char iter_name[64]; tpl_strcpy(iter_name,vt->text);
        tpl_advance(s);
        if (tpl_peek(s)->kind!=TK_NAME||tpl_strcmp(tpl_peek(s)->text,"in")!=0){
            tpl_error(s,"for: expected 'in'"); return result;
        }
        tpl_advance(s);
        TplVal from_v=tpl_expr(s);
        if(tpl_peek(s)->kind!=TK_NAME||tpl_strcmp(tpl_peek(s)->text,"to")!=0){
            tpl_error(s,"for: expected 'to'"); return result;
        }
        tpl_advance(s);
        TplVal to_v=tpl_expr(s);
        int32_t step=1;
        if(tpl_peek(s)->kind==TK_NAME&&tpl_strcmp(tpl_peek(s)->text,"step")==0){
            tpl_advance(s);
            TplVal sv=tpl_expr(s); step=tpl_val_num(&sv);
        }
        tpl_skip_newlines(s);
        tpl_expect(s,TK_COLON);
        tpl_skip_newlines(s);
        tpl_expect(s,TK_LBRACE);
        int body_start=s->tok_pos;
        int32_t from=tpl_val_num(&from_v), to=tpl_val_num(&to_v);
        if(step==0) step=1;
        for(int32_t i=from; step>0?i<=to:i>=to; i+=step){
            templar_set_var(s,iter_name,tpl_num(i));
            s->tok_pos=body_start;
            TplExec r=tpl_exec_block_ex(s);
            if(r.flag==TPL_RET_BREAK) break;
            if(r.flag==TPL_RET_RETURN){
                s->tok_pos=body_start;
                int depth=1;
                while(depth>0&&tpl_peek(s)->kind!=TK_EOF){
                    if(tpl_peek(s)->kind==TK_LBRACE)depth++;
                    else if(tpl_peek(s)->kind==TK_RBRACE)depth--;
                    tpl_advance(s);
                }
                return r;
            }
            if(s->error) break;
        }
        if(!s->error) {
            s->tok_pos=body_start;
            int depth=1;
            while(depth>0&&tpl_peek(s)->kind!=TK_EOF){
                if(tpl_peek(s)->kind==TK_LBRACE)depth++;
                else if(tpl_peek(s)->kind==TK_RBRACE)depth--;
                tpl_advance(s);
            }
        }
        return result;
    }

    if (t->kind==TK_NAME && tpl_strcmp(t->text,"return")==0) {
        tpl_advance(s);
        TplVal v = tpl_nil();
        if (tpl_peek(s)->kind!=TK_NEWLINE && tpl_peek(s)->kind!=TK_RBRACE && tpl_peek(s)->kind!=TK_EOF)
            v = tpl_expr(s);
        result.flag=TPL_RET_RETURN; result.val=v;
        return result;
    }

    if (t->kind==TK_NAME && tpl_strcmp(t->text,"break")==0) {
        tpl_advance(s);
        result.flag=TPL_RET_BREAK;
        return result;
    }

    if (t->kind==TK_NAME && tpl_strcmp(t->text,"continue")==0) {
        tpl_advance(s);
        result.flag=TPL_RET_CONT;
        return result;
    }

    if (t->kind==TK_NAME && tpl_strcmp(t->text,"fn")==0) {
        tpl_advance(s);
        TplTok *fname=tpl_peek(s);
        if(fname->kind!=TK_NAME){tpl_error(s,"fn: expected name");return result;}
        int fidx=s->func_count;
        if(fidx>=TEMPLAR_MAX_FUNCS){tpl_error(s,"too many functions");return result;}
        tpl_strcpy(s->funcs[fidx].name,fname->text);
        tpl_advance(s);
        tpl_expect(s,TK_LPAREN);
        s->funcs[fidx].param_count=0;
        while(tpl_peek(s)->kind!=TK_RPAREN&&tpl_peek(s)->kind!=TK_EOF){
            TplTok *pn=tpl_peek(s);
            if(pn->kind==TK_NAME){
                tpl_strcpy(s->funcs[fidx].params[s->funcs[fidx].param_count++],pn->text);
                tpl_advance(s);
            }
            tpl_match(s,TK_COMMA);
        }
        tpl_expect(s,TK_RPAREN);
        tpl_skip_newlines(s);
        tpl_expect(s,TK_COLON);
        tpl_skip_newlines(s);
        tpl_expect(s,TK_LBRACE);
        s->funcs[fidx].line=s->tok_pos;
        s->func_count++;
        int depth=1;
        while(depth>0&&tpl_peek(s)->kind!=TK_EOF){
            if(tpl_peek(s)->kind==TK_LBRACE)depth++;
            else if(tpl_peek(s)->kind==TK_RBRACE)depth--;
            tpl_advance(s);
        }
        return result;
    }

    tpl_expr(s);
    return result;
}

static TplExec tpl_exec_block_ex(TplState *s) {
    TplExec result; result.flag=TPL_RET_NONE; result.val=tpl_nil();
    while (!s->error) {
        tpl_skip_newlines(s);
        TplTok *t=tpl_peek(s);
        if(t->kind==TK_EOF||t->kind==TK_RBRACE) break;
        TplExec r=tpl_exec_stmt_ex(s);
        if(r.flag!=TPL_RET_NONE) return r;
    }
    return result;
}

int templar_run(TplState *s) {
    s->tok_pos=0;
    s->error=0;
    s->error_msg[0]=0;
    tpl_lexer(s);
    TplExec r=tpl_exec_block_ex(s);
    if(s->error){
        klog("Templar error: ");
        klog(s->error_msg);
        klog("\n");
        return -1;
    }
    return 0;
}

int templar_run_string(TplState *s, const char *code) {
    int len=tpl_strlen(code);
    if(len>=TEMPLAR_MAX_SRC) len=TEMPLAR_MAX_SRC-1;
    for(int i=0;i<len;i++) s->src[i]=code[i];
    s->src[len]=0;
    s->src_len=len;
    return templar_run(s);
}

int templar_load_file(TplState *s, const char *filename) {
    int inode_num=fs_resolve_path(filename,g_current_dir);
    if(inode_num<0){
        klog("templar: file not found: ");
        klog(filename); klog("\n");
        return -1;
    }
    inode_t node;
    read_inode(inode_num,&node);
    uint32_t sz=node.size;
    if(sz>=TEMPLAR_MAX_SRC) sz=TEMPLAR_MAX_SRC-1;
    fs_read((uint32_t)inode_num,&node,0,sz,(uint8_t*)s->src);
    s->src[sz]=0;
    s->src_len=(int)sz;
    return templar_run(s);
}
