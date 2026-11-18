#include "nlmap.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define CRS "http%3A%2F%2Fwww.opengis.net%2Fdef%2Fcrs%2FEPSG%2F0%2F28992"
#define MAXP 4096


/* ------------------------------------------------------------------ url's */

int nl_normalize_postcode(const char *in, char *out) {
    int n = 0, i;
    for (i = 0; in[i] && n < 6; i++) {
        char c = (char)toupper((unsigned char)in[i]);
        if (c == ' ') continue;
        out[n++] = c;
    }
    out[n] = 0;
    if (n != 6) return 0;
    for (i = 0; i < 4; i++) if (!isdigit((unsigned char)out[i])) return 0;
    if (out[0] == '0') return 0;
    for (i = 4; i < 6; i++) if (out[i] < 'A' || out[i] > 'Z') return 0;
    return 1;
}

void nl_url_geocode(char *buf, int n, const char *pc) {
    snprintf(buf, n, "https://api.pdok.nl/bzk/locatieserver/search/v3_1/free?q=postcode:%s&fq=type:postcode&rows=1&fl=centroide_rd,weergavenaam", pc);
}

void nl_url_bag3d(char *buf, int n, double cx, double cy) {
    snprintf(buf, n, "https://data.3dbag.nl/api/BAG3D/wfs?service=WFS&version=2.0.0&request=GetFeature&typeNames=BAG3D:lod12"
             "&outputFormat=application/json&srsName=EPSG:28992&bbox=%.0f,%.0f,%.0f,%.0f,EPSG:28992"
             "&propertyName=geom,b3_h_70p,b3_h_maaiveld&count=3000",
             cx - NL_HALF, cy - NL_HALF, cx + NL_HALF, cy + NL_HALF);
}

void nl_url_bagpdok(char *buf, int n, double cx, double cy) {
    snprintf(buf, n, "https://api.pdok.nl/kadaster/bag/ogc/v2/collections/pand/items?bbox=%.0f,%.0f,%.0f,%.0f"
             "&bbox-crs=%s&crs=%s&limit=1000&f=json",
             cx - NL_HALF, cy - NL_HALF, cx + NL_HALF, cy + NL_HALF, CRS, CRS);
}

void nl_url_top10(char *buf, int n, const char *collection, double cx, double cy) {
    snprintf(buf, n, "https://api.pdok.nl/brt/top10nl/ogc/v1/collections/%s/items?bbox=%.0f,%.0f,%.0f,%.0f"
             "&bbox-crs=%s&crs=%s&limit=1000&f=json",
             collection, cx - NL_HALF, cy - NL_HALF, cx + NL_HALF, cy + NL_HALF, CRS, CRS);
}

void nl_url_bag3d_bbox(char *buf, int n, double x0, double y0, double x1, double y1) {
    snprintf(buf, n, "https://data.3dbag.nl/api/BAG3D/wfs?service=WFS&version=2.0.0&request=GetFeature&typeNames=BAG3D:lod12"
             "&outputFormat=application/json&srsName=EPSG:28992&bbox=%.0f,%.0f,%.0f,%.0f,EPSG:28992"
             "&propertyName=geom,b3_h_70p,b3_h_maaiveld&count=3000", x0, y0, x1, y1);
}

void nl_url_bagpdok_bbox(char *buf, int n, double x0, double y0, double x1, double y1) {
    snprintf(buf, n, "https://api.pdok.nl/kadaster/bag/ogc/v2/collections/pand/items?bbox=%.0f,%.0f,%.0f,%.0f"
             "&bbox-crs=%s&crs=%s&limit=1000&f=json", x0, y0, x1, y1, CRS, CRS);
}

void nl_url_top10_bbox(char *buf, int n, const char *collection, double x0, double y0, double x1, double y1) {
    snprintf(buf, n, "https://api.pdok.nl/brt/top10nl/ogc/v1/collections/%s/items?bbox=%.0f,%.0f,%.0f,%.0f"
             "&bbox-crs=%s&crs=%s&limit=1000&f=json", collection, x0, y0, x1, y1, CRS, CRS);
}

int nl_parse_geocode(char *json, long len, NLGeo *out) {
    JArena a = {0};
    JV *root = json_parse(json, len, &a), *resp, *docs, *d;
    const char *pt, *nm;
    int ok = 0;
    memset(out, 0, sizeof(*out));
    resp = json_get(root, "response");
    docs = json_get(resp, "docs");
    d = docs ? docs->child : NULL;
    pt = json_str(json_get(d, "centroide_rd"));
    nm = json_str(json_get(d, "weergavenaam"));
    if (pt) {
        const char *p = strchr(pt, '(');
        if (p && sscanf(p + 1, "%lf %lf", &out->cx, &out->cy) == 2) ok = 1;
    }
    if (nm) snprintf(out->name, sizeof(out->name), "%s", nm);
    json_free(&a);
    return ok;
}

const char *nl_street_name(const World *w, float x, float z) {
    int i, best = -1;
    float bd = 14.0f * 14.0f;
    for (i = 0; i < w->nrsegs; i++) {
        const RSeg *r = &w->rsegs[i];
        float ex = r->x1 - r->x0, ez = r->z1 - r->z0, l2 = ex * ex + ez * ez, t, dx, dz, d2;
        if (r->name < 0) continue;
        t = l2 > 1e-6f ? clampf(((x - r->x0) * ex + (z - r->z0) * ez) / l2, 0, 1) : 0;
        dx = x - (r->x0 + ex * t); dz = z - (r->z0 + ez * t); d2 = dx * dx + dz * dz;
        if (d2 < bd) { bd = d2; best = r->name; }
    }
    return best >= 0 ? w->names + best : NULL;
}
