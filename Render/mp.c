#include "mp.h"
#include "netsock.h"
#include "platform.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

enum { P_HELLO = 1, P_WELCOME = 2, P_STATE = 3, P_HIT = 4, P_BYE = 5, P_BEACON = 6, P_FULL = 7 };

#pragma pack(push, 1)
typedef struct { char magic[4]; uint8_t type, id; uint16_t seq; } PHdr;
typedef struct {
    PHdr h;
    char name[16];
    float x, y, z, hd, speed, anim, health;
    uint8_t mode, vmodel, vpaint, vcustom, vvariant, weapon;
    uint16_t shots;
} PState;
typedef struct { PHdr h; uint8_t yourId, world; double ox, oy; char label[64]; } PWelcome;
typedef struct { PHdr h; uint8_t target; float dmg; char from[16]; } PHit;
typedef struct { PHdr h; char name[16]; uint8_t players, world; uint16_t port; } PBeacon;
#pragma pack(pop)

typedef struct { int used; uint32_t ip; int port; double seen; char name[16]; } Peer;

static struct {
    int mode, sock, beaconSock, myId;
    char name[16];
    int world; double ox, oy; char label[64];
    Peer peer[4];                 /* host: index = speler-id (1..3); client: [0] = host */
    double t, lastSend, lastBeacon, lastHello, joinStart, lastStun;
    char status[160], codeNet[16], codeLan[16], note[200], event[96];
    int haveWorld, eventNew;
    MPLan lan[MP_MAXLAN]; int nlan, lanOn;
    uint16_t seq;
    /* UPnP/STUN (achtergrond) */
    volatile int upnpDone, upnpOk;
    uint32_t pubIp, stunIp, localIp; int stunPort;
    char ctrlHost[64], ctrlPath[256], svcType[96]; uint32_t ctrlIp; int ctrlPort;
    Game *g;
} M;

static double now(void) { return plat_time(); }

void mp_set_name(const char *n) { snprintf(M.name, sizeof(M.name), "%s", n); }
const char *mp_name(void) { return M.name; }
int mp_mode(void) { return M.mode; }
const char *mp_status(void) { return M.status; }
const char *mp_code_internet(void) { return M.codeNet; }
const char *mp_code_lan(void) { return M.codeLan; }
const char *mp_internet_note(void) { return M.note; }
int mp_lan_count(void) { return M.nlan; }
const MPLan *mp_lan(int i) { return i >= 0 && i < M.nlan ? &M.lan[i] : NULL; }

const char *mp_event(void) {
    if (!M.eventNew) return NULL;
    M.eventNew = 0;
    return M.event;
}

static void event(const char *fmt, const char *a) {
    snprintf(M.event, sizeof(M.event), fmt, a);
    M.eventNew = 1;
    plat_log("SGB-MP %s", M.event);
}

int mp_players(void) {
    int i, n = 1;
    if (M.mode == MP_HOST) { for (i = 1; i < 4; i++) n += M.peer[i].used; return n; }
    if (M.mode == MP_CLIENT && M.g) { for (i = 0; i < MAX_REMOTE; i++) n += M.g->remote[i].active; return n; }
    return M.mode ? 1 : 0;
}

/* ---- uitnodigingscode: IPv4 + poort (48 bits) in 10 tekens base32 */
static const char ALPH[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

static void code_make(uint32_t ip, int port, char *out) {
    uint64_t v = ((uint64_t)ip << 16) | (uint64_t)(port & 0xFFFF);
    char t[11];
    int i;
    for (i = 9; i >= 0; i--) { t[i] = ALPH[v & 31]; v >>= 5; }
    snprintf(out, 16, "%.5s-%.5s", t, t + 5);
}

int mp_code_decode(const char *code, uint32_t *ip, int *port) {
    uint64_t v = 0;
    int n = 0;
    for (; *code && n < 10; code++) {
        const char *p;
        char c = (char)toupper((unsigned char)*code);
        if (c == '-' || c == ' ') continue;
        if (c == '0') c = 'O';
        if (c == '1') c = 'I';
        p = strchr(ALPH, c);
        if (!p) return 0;
        v = (v << 5) | (uint64_t)(p - ALPH);
        n++;
    }
    if (n != 10) return 0;
    *ip = (uint32_t)(v >> 16);
    *port = (int)(v & 0xFFFF);
    return *ip != 0 && *port != 0;
}

/* ---- pakketten */
static void hdr(PHdr *h, int type) { memcpy(h->magic, "SGB1", 4); h->type = (uint8_t)type; h->id = (uint8_t)M.myId; h->seq = ++M.seq; }

static void fill_state(PState *s, const Game *g) {
    memset(s, 0, sizeof(*s));
    hdr(&s->h, P_STATE);
    snprintf(s->name, sizeof(s->name), "%s", M.name);
    s->x = g->p.x; s->z = g->p.z; s->y = g->p.y; s->hd = g->p.h; s->anim = g->p.anim; s->health = g->health;
    s->weapon = (uint8_t)g->weapon; s->shots = (uint16_t)g->statShots;
    if (g->p.veh >= 0) {
        const Vehicle *v = &g->veh[g->p.veh];
        s->mode = v->type == VEH_BIKE ? 1 : 2;
        s->speed = v->speed; s->hd = v->h; s->x = v->x; s->z = v->z; s->y = v->y;
        s->vmodel = (uint8_t)v->model; s->vpaint = (uint8_t)v->paint; s->vcustom = (uint8_t)v->custom; s->vvariant = (uint8_t)v->variant;
    } else { s->mode = 0; s->speed = g->p.speed; }
}

static RemotePlayer *remote_for(Game *g, int id, int create) {
    int i, free = -1;
    for (i = 0; i < MAX_REMOTE; i++) {
        if (g->remote[i].active && g->remote[i].id == id) return &g->remote[i];
        if (!g->remote[i].active && free < 0) free = i;
    }
    if (!create || free < 0) return NULL;
    memset(&g->remote[free], 0, sizeof(RemotePlayer));
    g->remote[free].active = 1;
    g->remote[free].id = id;
    g->remote[free].fresh = 1;
    return &g->remote[free];
}

static void apply_state(Game *g, const PState *s, int id) {
    RemotePlayer *r = remote_for(g, id, 1);
    if (!r) return;
    if (!r->name[0]) event("%s joined the game", s->name);
    snprintf(r->name, sizeof(r->name), "%s", s->name);
    r->tx = s->x; r->ty = s->y; r->tz = s->z; r->th = s->hd;
    if (r->fresh) { r->x = s->x; r->y = s->y; r->z = s->z; r->h = s->hd; r->fresh = 0; }
    r->speed = s->speed; r->anim = s->anim; r->health = s->health;
    r->mode = s->mode; r->vmodel = s->vmodel; r->vpaint = s->vpaint; r->vcustom = s->vcustom; r->vvariant = s->vvariant;
    r->weapon = s->weapon;
    if (s->shots != r->shots && r->shotsSeen) {
        game_sfx(g, s->weapon == WP_SMG ? SFX_SMG : (s->weapon == WP_SHOTGUN ? SFX_SHOTGUN : SFX_PISTOL), r->x, r->z);
        r->flash = 0.08f;
    }
    r->shots = s->shots; r->shotsSeen = 1;
    r->seen = (float)now();
}

static void remove_remote(Game *g, int id) {
    RemotePlayer *r = remote_for(g, id, 0);
    if (r) { event("%s left the game", r->name[0] ? r->name : "A player"); r->active = 0; }
}

/* ---- UPnP + STUN (achtergrond-thread, alleen de host) */

static int find_ci(const char *hay, const char *needle) {
    size_t n = strlen(needle);
    const char *p;
    for (p = hay; *p; p++) {
        size_t k;
        for (k = 0; k < n && p[k] && tolower((unsigned char)p[k]) == tolower((unsigned char)needle[k]); k++) {}
        if (k == n) return (int)(p - hay);
    }
    return -1;
}

static int xml_tag(const char *xml, const char *tag, char *out, int n) {
    char open[64], close[64];
    const char *a, *b;
    snprintf(open, sizeof(open), "<%s>", tag); snprintf(close, sizeof(close), "</%s>", tag);
    a = strstr(xml, open);
    if (!a) return 0;
    a += strlen(open);
    b = strstr(a, close);
    if (!b || b - a >= n) return 0;
    memcpy(out, a, (size_t)(b - a)); out[b - a] = 0;
    return 1;
}

static int parse_url(const char *url, uint32_t *ip, int *port, char *path, int pn) {
    unsigned a, b, c, d, p = 80;
    const char *s = strstr(url, "://");
    if (!s) return 0;
    s += 3;
    if (sscanf(s, "%u.%u.%u.%u:%u", &a, &b, &c, &d, &p) < 4) return 0;
    *ip = (a << 24) | (b << 16) | (c << 8) | d;
    *port = (int)p;
    s = strchr(s, '/');
    snprintf(path, (size_t)pn, "%s", s ? s : "/");
    return 1;
}

static int soap(const char *action, const char *args, char *resp, int cap) {
    char body[1400], req[2200];
    snprintf(body, sizeof(body),
             "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
             "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>",
             action, M.svcType, args, action);
    snprintf(req, sizeof(req),
             "POST %s HTTP/1.1\r\nHOST: %s\r\nCONTENT-TYPE: text/xml; charset=\"utf-8\"\r\nSOAPACTION: \"%s#%s\"\r\n"
             "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
             M.ctrlPath, M.ctrlHost, M.svcType, action, (int)strlen(body), body);
    return us_http(M.ctrlIp, M.ctrlPort, req, resp, cap, 3000);
}

static void *upnp_thread(void *arg) {
    static char buf[16384];
    char loc[256], path[256], ipstr[32], args[600];
    uint32_t dip;
    int dport, n, i;
    (void)arg;
    M.localIp = us_local_ip();
    M.stunIp = us_resolve("stun.l.google.com");
    n = us_ssdp_search(buf, sizeof(buf), 2400);
    i = n > 0 ? find_ci(buf, "location:") : -1;
    if (i >= 0) {
        const char *p = buf + i + 9;
        int k = 0;
        while (*p == ' ') p++;
        while (*p && *p != '\r' && *p != '\n' && k < (int)sizeof(loc) - 1) loc[k++] = *p++;
        loc[k] = 0;
        if (parse_url(loc, &dip, &dport, path, sizeof(path))) {
            char req[512];
            snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHOST: %u.%u.%u.%u:%d\r\nConnection: close\r\n\r\n", path,
                     (dip >> 24) & 255, (dip >> 16) & 255, (dip >> 8) & 255, dip & 255, dport);
            if (us_http(dip, dport, req, buf, sizeof(buf), 3000) > 0) {
                const char *svc = strstr(buf, "urn:schemas-upnp-org:service:WANIPConnection:");
                if (!svc) svc = strstr(buf, "urn:schemas-upnp-org:service:WANPPPConnection:");
                if (svc) {
                    char ctrl[256];
                    int k2 = 0;
                    while (svc[k2] && svc[k2] != '<' && k2 < (int)sizeof(M.svcType) - 1) { M.svcType[k2] = svc[k2]; k2++; }
                    M.svcType[k2] = 0;
                    if (xml_tag(svc, "controlURL", ctrl, sizeof(ctrl))) {
                        M.ctrlIp = dip; M.ctrlPort = dport;
                        snprintf(M.ctrlHost, sizeof(M.ctrlHost), "%u.%u.%u.%u:%d", (dip >> 24) & 255, (dip >> 16) & 255, (dip >> 8) & 255, dip & 255, dport);
                        if (strncmp(ctrl, "http", 4) == 0) { uint32_t x; int y; parse_url(ctrl, &x, &y, M.ctrlPath, sizeof(M.ctrlPath)); }
                        else snprintf(M.ctrlPath, sizeof(M.ctrlPath), "%s", ctrl);
                        us_ip_str(M.localIp, ipstr, sizeof(ipstr));
                        snprintf(args, sizeof(args),
                                 "<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>UDP</NewProtocol>"
                                 "<NewInternalPort>%d</NewInternalPort><NewInternalClient>%s</NewInternalClient><NewEnabled>1</NewEnabled>"
                                 "<NewPortMappingDescription>SmallGiveawayBike1</NewPortMappingDescription><NewLeaseDuration>7200</NewLeaseDuration>",
                                 MP_PORT, MP_PORT, ipstr);
                        if (soap("AddPortMapping", args, buf, sizeof(buf)) > 0 && strstr(buf, "AddPortMappingResponse")) M.upnpOk = 1;
                        if (soap("GetExternalIPAddress", "", buf, sizeof(buf)) > 0) {
                            char ext[32];
                            unsigned a, b, c, d;
                            if (xml_tag(buf, "NewExternalIPAddress", ext, sizeof(ext)) && sscanf(ext, "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
                                M.pubIp = (a << 24) | (b << 16) | (c << 8) | d;
                        }
                    }
                }
            }
        }
    }
    plat_log("SGB-MP UPnP %s, router-WAN-IP %u.%u.%u.%u", M.upnpOk ? "gelukt" : "niet gelukt",
             (M.pubIp >> 24) & 255, (M.pubIp >> 16) & 255, (M.pubIp >> 8) & 255, M.pubIp & 255);
    M.upnpDone = 1;
    return NULL;
}

static void *upnp_delete_thread(void *arg) {
    static char buf[4096];
    char args[300];
    (void)arg;
    snprintf(args, sizeof(args), "<NewRemoteHost></NewRemoteHost><NewExternalPort>%d</NewExternalPort><NewProtocol>UDP</NewProtocol>", MP_PORT);
    soap("DeletePortMapping", args, buf, sizeof(buf));
    return NULL;
}

static void stun_send(void) {
    unsigned char req[20] = {0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xA4, 0x42};
    int i;
    for (i = 8; i < 20; i++) req[i] = (unsigned char)(rand() & 255);
    if (M.stunIp) us_send(M.sock, M.stunIp, 19302, req, 20);
}

static int stun_parse(const unsigned char *b, int n) {
    int off = 20;
    if (n < 20 || b[0] != 0x01 || b[1] != 0x01 || b[4] != 0x21 || b[5] != 0x12) return 0;
    while (off + 4 <= n) {
        int type = (b[off] << 8) | b[off + 1], len = (b[off + 2] << 8) | b[off + 3];
        if ((type == 0x0020 || type == 0x0001) && len >= 8 && b[off + 5] == 0x01) {
            int port = (b[off + 6] << 8) | b[off + 7];
            uint32_t ip = ((uint32_t)b[off + 8] << 24) | ((uint32_t)b[off + 9] << 16) | ((uint32_t)b[off + 10] << 8) | b[off + 11];
            if (type == 0x0020) { port ^= 0x2112; ip ^= 0x2112A442u; }
            M.stunPort = port;
            if (!M.pubIp || !M.upnpOk) M.pubIp = ip;
            return 1;
        }
        off += 4 + ((len + 3) & ~3);
    }
    return 0;
}

static void update_codes(void) {
    char ip[32];
    if (M.localIp) code_make(M.localIp, MP_PORT, M.codeLan);
    if (M.upnpOk && M.pubIp) {
        code_make(M.pubIp, MP_PORT, M.codeNet);
        snprintf(M.note, sizeof(M.note), "Your router opened port %d automatically (UPnP). Friends anywhere can join with the internet code.", MP_PORT);
    } else if (M.pubIp && M.stunPort) {
        code_make(M.pubIp, M.stunPort, M.codeNet);
        us_ip_str(M.localIp, ip, sizeof(ip));
        snprintf(M.note, sizeof(M.note), "Router did not open the port (no UPnP). Joining over the internet may fail: forward UDP port %d to %s in your router.", MP_PORT, ip);
    } else if (M.upnpDone) {
        snprintf(M.note, sizeof(M.note), "No internet code (offline?). Players on the same Wi-Fi can still join.");
    }
}

/* ---- starten / stoppen */

int mp_host(int world, double ox, double oy, const char *label) {
    pthread_t th;
    mp_leave();
    us_init();
    M.sock = us_open(MP_PORT, 1);
    if (M.sock < 0) M.sock = us_open(0, 1);
    if (M.sock < 0) { snprintf(M.status, sizeof(M.status), "Could not open a network port."); return 0; }
    M.mode = MP_HOST; M.myId = 0;
    M.world = world; M.ox = ox; M.oy = oy; snprintf(M.label, sizeof(M.label), "%s", label ? label : "");
    memset(M.peer, 0, sizeof(M.peer));
    M.upnpDone = M.upnpOk = 0; M.pubIp = M.stunPort = 0; M.codeNet[0] = M.codeLan[0] = 0; M.note[0] = 0;
    M.localIp = us_local_ip();
    update_codes();
    snprintf(M.status, sizeof(M.status), "Hosting on port %d - looking up your internet address...", us_bound_port(M.sock));
    if (pthread_create(&th, NULL, upnp_thread, NULL) == 0) pthread_detach(th);
    plat_log("SGB-MP host gestart op poort %d (wereld %d)", us_bound_port(M.sock), world);
    return 1;
}

int mp_join(uint32_t ip, int port) {
    mp_leave();
    us_init();
    M.sock = us_open(0, 0);
    if (M.sock < 0) { snprintf(M.status, sizeof(M.status), "Could not open a network port."); return 0; }
    M.mode = MP_JOINING; M.myId = -1;
    memset(M.peer, 0, sizeof(M.peer));
    M.peer[0].used = 1; M.peer[0].ip = ip; M.peer[0].port = port; M.peer[0].seen = now();
    M.joinStart = now(); M.lastHello = 0; M.haveWorld = 0;
    {
        char s[32];
        us_ip_str(ip, s, sizeof(s));
        snprintf(M.status, sizeof(M.status), "Connecting to %s:%d ...", s, port);
    }
    return 1;
}

void mp_leave(void) {
    int i;
    if (M.mode != MP_OFF && M.sock >= 0) {
        PHdr b;
        hdr(&b, P_BYE);
        for (i = 0; i < 4; i++) if (M.peer[i].used) us_send(M.sock, M.peer[i].ip, M.peer[i].port, &b, sizeof(b));
    }
    if (M.mode == MP_HOST && M.upnpOk) { pthread_t th; if (pthread_create(&th, NULL, upnp_delete_thread, NULL) == 0) pthread_detach(th); }
    if (M.mode != MP_OFF) us_close(M.sock);
    M.sock = -1;
    M.mode = MP_OFF;
    M.status[0] = 0;
    if (M.g) { for (i = 0; i < MAX_REMOTE; i++) M.g->remote[i].active = 0; }
}

int mp_take_world(int *world, double *ox, double *oy, char *label, int n) {
    if (!M.haveWorld) return 0;
    M.haveWorld = 0;
    *world = M.world; *ox = M.ox; *oy = M.oy;
    snprintf(label, (size_t)n, "%s", M.label);
    return 1;
}

void mp_lan_listen(int on) {
    if (on && !M.lanOn) {
        us_init();
        M.beaconSock = us_open(MP_BEACON_PORT, 1);
        M.lanOn = M.beaconSock >= 0;
        M.nlan = 0;
    } else if (!on && M.lanOn) {
        us_close(M.beaconSock);
        M.lanOn = 0;
    }
}

/* ---- per frame */

static void send_to_all(const void *p, int len, int except) {
    int i;
    for (i = 1; i < 4; i++) if (M.peer[i].used && i != except) us_send(M.sock, M.peer[i].ip, M.peer[i].port, p, len);
}

void mp_tick(Game *g, float dt) {
    unsigned char buf[512];
    uint32_t ip;
    int port, n, i, guard;
    double t = now();
    M.g = g;
    /* LAN-bakens zoeken */
    if (M.lanOn) {
        for (guard = 0; guard < 32 && (n = us_recv(M.beaconSock, buf, sizeof(buf), &ip, &port)) > 0; guard++) {
            const PBeacon *b = (const PBeacon *)buf;
            int k;
            if (n < (int)sizeof(PBeacon) || memcmp(b->h.magic, "SGB1", 4) != 0 || b->h.type != P_BEACON) continue;
            if (M.mode == MP_HOST && b->port == us_bound_port(M.sock) && ip == M.localIp) continue;   /* eigen baken */
            for (k = 0; k < M.nlan; k++) if (M.lan[k].ip == ip && M.lan[k].port == b->port) break;
            if (k >= M.nlan) { if (M.nlan >= MP_MAXLAN) continue; k = M.nlan++; }
            snprintf(M.lan[k].name, sizeof(M.lan[k].name), "%s", b->name);
            M.lan[k].ip = ip; M.lan[k].port = b->port; M.lan[k].players = b->players; M.lan[k].world = b->world; M.lan[k].seen = t;
        }
        for (i = 0; i < M.nlan; i++) if (t - M.lan[i].seen > 4.0) { memmove(&M.lan[i], &M.lan[i + 1], sizeof(MPLan) * (size_t)(M.nlan - i - 1)); M.nlan--; i--; }
    }
    if (M.mode == MP_OFF) return;

    /* ontvangen */
    for (guard = 0; guard < 64 && (n = us_recv(M.sock, buf, sizeof(buf), &ip, &port)) > 0; guard++) {
        const PHdr *h = (const PHdr *)buf;
        if (n >= 20 && buf[0] == 0x01 && buf[1] == 0x01) { if (stun_parse(buf, n)) update_codes(); continue; }
        if (n < (int)sizeof(PHdr) || memcmp(h->magic, "SGB1", 4) != 0) continue;
        if (M.mode == MP_HOST) {
            int id = -1;
            for (i = 1; i < 4; i++) if (M.peer[i].used && M.peer[i].ip == ip && M.peer[i].port == port) id = i;
            if (h->type == P_HELLO) {
                PWelcome w;
                if (id < 0) {
                    for (i = 1; i < 4 && M.peer[i].used; i++) {}
                    if (i >= 4) { PHdr f; hdr(&f, P_FULL); us_send(M.sock, ip, port, &f, sizeof(f)); continue; }
                    id = i;
                    M.peer[id].used = 1; M.peer[id].ip = ip; M.peer[id].port = port;
                    if (n >= (int)sizeof(PHdr) + 16) snprintf(M.peer[id].name, sizeof(M.peer[id].name), "%.15s", (const char *)buf + sizeof(PHdr));
                    plat_log("SGB-MP speler %d verbonden", id);
                }
                M.peer[id].seen = t;
                memset(&w, 0, sizeof(w));
                hdr(&w.h, P_WELCOME);
                w.yourId = (uint8_t)id; w.world = (uint8_t)M.world; w.ox = M.ox; w.oy = M.oy;
                snprintf(w.label, sizeof(w.label), "%s", M.label);
                us_send(M.sock, ip, port, &w, sizeof(w));
            } else if (id >= 0) {
                M.peer[id].seen = t;
                if (h->type == P_STATE && n >= (int)sizeof(PState)) {
                    PState s = *(const PState *)buf;
                    s.h.id = (uint8_t)id;
                    apply_state(g, &s, id);
                    send_to_all(&s, sizeof(s), id);           /* doorgeven aan de anderen */
                } else if (h->type == P_HIT && n >= (int)sizeof(PHit)) {
                    const PHit *hit = (const PHit *)buf;
                    if (hit->target == 0) {
                        game_damage(g, hit->dmg);
                        { char m[64]; snprintf(m, sizeof(m), "Hit by %.15s!", hit->from); game_message(g, m, 1.5f); }
                    } else if (hit->target < 4 && M.peer[hit->target].used) {
                        us_send(M.sock, M.peer[hit->target].ip, M.peer[hit->target].port, hit, sizeof(PHit));
                    }
                } else if (h->type == P_BYE) {
                    M.peer[id].used = 0;
                    remove_remote(g, id);
                }
            }
        } else {
            if (ip != M.peer[0].ip || port != M.peer[0].port) continue;
            M.peer[0].seen = t;
            if (h->type == P_WELCOME && n >= (int)sizeof(PWelcome) && M.mode == MP_JOINING) {
                const PWelcome *w = (const PWelcome *)buf;
                M.myId = w->yourId; M.world = w->world; M.ox = w->ox; M.oy = w->oy;
                snprintf(M.label, sizeof(M.label), "%s", w->label);
                M.mode = MP_CLIENT; M.haveWorld = 1;
                snprintf(M.status, sizeof(M.status), "Connected as player %d", M.myId + 1);
                plat_log("SGB-MP verbonden met de host als speler %d (wereld %d)", M.myId, M.world);
            } else if (h->type == P_FULL) {
                snprintf(M.status, sizeof(M.status), "That game is full (4 players).");
                mp_leave();
                return;
            } else if (h->type == P_STATE && n >= (int)sizeof(PState) && M.mode == MP_CLIENT) {
                const PState *s = (const PState *)buf;
                if (s->h.id != M.myId) apply_state(g, s, s->h.id);
            } else if (h->type == P_HIT && n >= (int)sizeof(PHit) && M.mode == MP_CLIENT) {
                const PHit *hit = (const PHit *)buf;
                if (hit->target == M.myId) {
                    char m[64];
                    game_damage(g, hit->dmg);
                    snprintf(m, sizeof(m), "Hit by %.15s!", hit->from);
                    game_message(g, m, 1.5f);
                }
            } else if (h->type == P_BYE) {
                snprintf(M.status, sizeof(M.status), "The host left the game.");
                event("%s", "The host ended the game");
                mp_leave();
                return;
            }
        }
    }

    /* versturen */
    if (M.mode == MP_JOINING) {
        if (t - M.lastHello > 0.4) {
            unsigned char hello[sizeof(PHdr) + 16];
            hdr((PHdr *)hello, P_HELLO);
            memset(hello + sizeof(PHdr), 0, 16);
            snprintf((char *)hello + sizeof(PHdr), 16, "%s", M.name);
            us_send(M.sock, M.peer[0].ip, M.peer[0].port, hello, sizeof(hello));
            M.lastHello = t;
        }
        if (t - M.joinStart > 12.0) {
            snprintf(M.status, sizeof(M.status), "No answer from the host. Check the code, or the host's router (UDP port %d).", MP_PORT);
            mp_leave();
        }
        return;
    }
    if (t - M.lastSend > 1.0 / 15.0) {
        PState s;
        fill_state(&s, g);
        if (M.mode == MP_HOST) send_to_all(&s, sizeof(s), -1);
        else us_send(M.sock, M.peer[0].ip, M.peer[0].port, &s, sizeof(s));
        M.lastSend = t;
    }
    /* treffers op andere spelers versturen */
    for (i = 0; i < g->nhit; i++) {
        PHit hit;
        memset(&hit, 0, sizeof(hit));
        hdr(&hit.h, P_HIT);
        hit.target = (uint8_t)g->hitq[i].target; hit.dmg = g->hitq[i].dmg;
        snprintf(hit.from, sizeof(hit.from), "%s", M.name);
        if (M.mode == MP_HOST) { if (hit.target < 4 && M.peer[hit.target].used) us_send(M.sock, M.peer[hit.target].ip, M.peer[hit.target].port, &hit, sizeof(hit)); }
        else us_send(M.sock, M.peer[0].ip, M.peer[0].port, &hit, sizeof(hit));
    }
    g->nhit = 0;
    if (M.mode == MP_HOST) {
        if (t - M.lastBeacon > 1.0) {
            PBeacon b;
            memset(&b, 0, sizeof(b));
            hdr(&b.h, P_BEACON);
            snprintf(b.name, sizeof(b.name), "%s", M.name);
            b.players = (uint8_t)mp_players(); b.world = (uint8_t)M.world; b.port = (uint16_t)us_bound_port(M.sock);
            us_send(M.sock, 0xFFFFFFFFu, MP_BEACON_PORT, &b, sizeof(b));
            M.lastBeacon = t;
        }
        if (!M.stunPort && M.stunIp && t - M.lastStun > 2.0) { stun_send(); M.lastStun = t; }
        if (M.upnpDone) update_codes();
        snprintf(M.status, sizeof(M.status), "Hosting - %d player%s", mp_players(), mp_players() == 1 ? "" : "s");
        for (i = 1; i < 4; i++) if (M.peer[i].used && t - M.peer[i].seen > 6.0) { M.peer[i].used = 0; remove_remote(g, i); }
    } else if (t - M.peer[0].seen > 8.0) {
        snprintf(M.status, sizeof(M.status), "Lost the connection to the host.");
        event("%s", "Connection to the host lost");
        mp_leave();
        return;
    }
    /* andere spelers soepel bewegen */
    for (i = 0; i < MAX_REMOTE; i++) {
        RemotePlayer *r = &g->remote[i];
        float k = clampf(dt * 10.0f, 0, 1);
        if (!r->active) continue;
        r->x = lerpf(r->x, r->tx, k); r->y = lerpf(r->y, r->ty, k); r->z = lerpf(r->z, r->tz, k);
        r->h = approach_angle(r->h, r->th, 8.0f * dt);
        r->animLocal += fabsf(r->speed) * dt * 2.3f;
        if (r->flash > 0) r->flash -= dt;
        if (M.mode == MP_CLIENT && t - r->seen > 6.0) r->active = 0;
    }
}
