/* Minimale JSON-lezer (in-place, arena). Genoeg voor GeoJSON van PDOK en 3D BAG. */
#ifndef SGB_JSON_H
#define SGB_JSON_H

enum { J_NULL = 0, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ };

typedef struct JV {
    int type;
    double num;
    const char *str;   /* bij J_STR */
    const char *key;   /* naam binnen een object */
    struct JV *child, *next;
    int count;
} JV;

typedef struct JBlock { struct JBlock *prev; int used, cap; JV nodes[1]; } JBlock;
typedef struct { JBlock *top; } JArena;

/* buf wordt aangepast (strings worden afgesloten). Geeft NULL bij fout. */
JV *json_parse(char *buf, long len, JArena *a);
void json_free(JArena *a);
JV *json_get(const JV *obj, const char *key);
double json_num(const JV *v, double def);
const char *json_str(const JV *v);

#endif
