#include "json.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char *p, *end; JArena *a; int depth; } JP;

static JV *node(JArena *a) {
    JV *v;
    if (!a->top || a->top->used >= a->top->cap) {
        int cap = 8192;
        JBlock *b = (JBlock *)malloc(sizeof(JBlock) + sizeof(JV) * (cap - 1));
        if (!b) return NULL;
        b->prev = a->top; b->used = 0; b->cap = cap;
        a->top = b;
    }
    v = &a->top->nodes[a->top->used++];
    memset(v, 0, sizeof(*v));
    return v;
}

void json_free(JArena *a) {
    while (a->top) { JBlock *p = a->top->prev; free(a->top); a->top = p; }
}

static void ws(JP *p) { while (p->p < p->end && (*p->p == ' ' || *p->p == '\n' || *p->p == '\r' || *p->p == '\t')) p->p++; }

static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

/* leest string in-place; p->p staat op het openingsaanhalingsteken */
static char *pstr(JP *p) {
    char *out, *start;
    p->p++;
    start = out = p->p;
    while (p->p < p->end && *p->p != '"') {
        if (*p->p == '\\' && p->p + 1 < p->end) {
            char c = p->p[1];
            p->p += 2;
            switch (c) {
            case 'n': *out++ = '\n'; break;
            case 't': *out++ = '\t'; break;
            case 'r': *out++ = '\r'; break;
            case 'b': *out++ = '\b'; break;
            case 'f': *out++ = '\f'; break;
            case 'u': {
                unsigned cp = 0;
                int k;
                for (k = 0; k < 4 && p->p < p->end; k++) cp = cp * 16 + (unsigned)hexv(*p->p++);
                if (cp < 0x80) *out++ = (char)cp;
                else if (cp < 0x800) { *out++ = (char)(0xC0 | (cp >> 6)); *out++ = (char)(0x80 | (cp & 0x3F)); }
                else { *out++ = (char)(0xE0 | (cp >> 12)); *out++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *out++ = (char)(0x80 | (cp & 0x3F)); }
                break;
            }
            default: *out++ = c; break;
            }
        } else *out++ = *p->p++;
    }
    if (p->p < p->end) p->p++;
    *out = 0;
    return start;
}

static JV *pval(JP *p);

static JV *pval(JP *p) {
    JV *v;
    ws(p);
    if (p->p >= p->end || p->depth > 64) return NULL;
    v = node(p->a);
    if (!v) return NULL;
    switch (*p->p) {
    case '{': case '[': {
        int obj = *p->p == '{';
        JV *last = NULL;
        v->type = obj ? J_OBJ : J_ARR;
        p->p++;
        p->depth++;
        for (;;) {
            const char *key = NULL;
            JV *c;
            ws(p);
            if (p->p >= p->end) return NULL;
            if (*p->p == (obj ? '}' : ']')) { p->p++; break; }
            if (obj) {
                if (*p->p != '"') return NULL;
                key = pstr(p);
                ws(p);
                if (p->p >= p->end || *p->p != ':') return NULL;
                p->p++;
            }
            c = pval(p);
            if (!c) return NULL;
            c->key = key;
            if (last) last->next = c; else v->child = c;
            last = c;
            v->count++;
            ws(p);
            if (p->p < p->end && *p->p == ',') p->p++;
        }
        p->depth--;
        return v;
    }
    case '"':
        v->type = J_STR;
        v->str = pstr(p);
        return v;
    case 't': v->type = J_BOOL; v->num = 1; p->p += 4; return v;
    case 'f': v->type = J_BOOL; v->num = 0; p->p += 5; return v;
    case 'n': v->type = J_NULL; p->p += 4; return v;
    default: {
        char *e;
        v->type = J_NUM;
        v->num = strtod(p->p, &e);
        if (e == p->p) return NULL;
        p->p = e;
        return v;
    }
    }
}

JV *json_parse(char *buf, long len, JArena *a) {
    JP p;
    p.p = buf; p.end = buf + len; p.a = a; p.depth = 0;
    return pval(&p);
}

JV *json_get(const JV *obj, const char *key) {
    JV *c;
    if (!obj || obj->type != J_OBJ) return NULL;
    for (c = obj->child; c; c = c->next) if (c->key && strcmp(c->key, key) == 0) return c;
    return NULL;
}

double json_num(const JV *v, double def) { return (v && (v->type == J_NUM || v->type == J_BOOL)) ? v->num : def; }
const char *json_str(const JV *v) { return (v && v->type == J_STR) ? v->str : NULL; }
