#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "netsock.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET SOCK;
#define SOCK_BAD INVALID_SOCKET
#define CLOSESOCK closesocket
static void set_nonblock(SOCK s) { u_long m = 1; ioctlsocket(s, FIONBIO, &m); }
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
typedef int SOCK;
#define SOCK_BAD (-1)
#define CLOSESOCK close
static void set_nonblock(SOCK s) { int f = fcntl(s, F_GETFL, 0); fcntl(s, F_SETFL, f | O_NONBLOCK); }
#endif

#define MAXS 8
static SOCK socks[MAXS];
static int used[MAXS], inited;

int us_init(void) {
    if (inited) return 1;
#ifdef _WIN32
    {
        WSADATA w;
        if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return 0;
    }
#endif
    inited = 1;
    return 1;
}

int us_open(int port, int broadcast) {
    SOCK s;
    struct sockaddr_in a;
    int i, one = 1;
    us_init();
    for (i = 0; i < MAXS && used[i]; i++) {}
    if (i >= MAXS) return -1;
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == SOCK_BAD) return -1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
    if (broadcast) setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof(one));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) != 0) { CLOSESOCK(s); return -1; }
    set_nonblock(s);
    socks[i] = s; used[i] = 1;
    return i;
}

void us_close(int h) {
    if (h < 0 || h >= MAXS || !used[h]) return;
    CLOSESOCK(socks[h]);
    used[h] = 0;
}

int us_send(int h, uint32_t ip, int port, const void *data, int len) {
    struct sockaddr_in a;
    if (h < 0 || h >= MAXS || !used[h]) return -1;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(ip);
    return (int)sendto(socks[h], (const char *)data, len, 0, (struct sockaddr *)&a, sizeof(a));
}

int us_recv(int h, void *buf, int cap, uint32_t *ip, int *port) {
    struct sockaddr_in a;
#ifdef _WIN32
    int al = sizeof(a);
#else
    socklen_t al = sizeof(a);
#endif
    int n;
    if (h < 0 || h >= MAXS || !used[h]) return 0;
    n = (int)recvfrom(socks[h], (char *)buf, cap, 0, (struct sockaddr *)&a, &al);
    if (n <= 0) return 0;
    if (ip) *ip = ntohl(a.sin_addr.s_addr);
    if (port) *port = ntohs(a.sin_port);
    return n;
}

int us_bound_port(int h) {
    struct sockaddr_in a;
#ifdef _WIN32
    int al = sizeof(a);
#else
    socklen_t al = sizeof(a);
#endif
    if (h < 0 || h >= MAXS || !used[h]) return 0;
    if (getsockname(socks[h], (struct sockaddr *)&a, &al) != 0) return 0;
    return ntohs(a.sin_port);
}

uint32_t us_local_ip(void) {
    /* truc: "verbind" een UDP-socket met een openbaar adres en lees het eigen adres uit (er wordt niets verstuurd) */
    SOCK s;
    struct sockaddr_in a;
    uint32_t ip = 0;
#ifdef _WIN32
    int al = sizeof(a);
#else
    socklen_t al = sizeof(a);
#endif
    us_init();
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == SOCK_BAD) return 0;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(53);
    a.sin_addr.s_addr = htonl(0x08080808u);
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) == 0 && getsockname(s, (struct sockaddr *)&a, &al) == 0) ip = ntohl(a.sin_addr.s_addr);
    CLOSESOCK(s);
    return ip;
}

uint32_t us_resolve(const char *host) {
    struct addrinfo hints, *res = NULL;
    uint32_t ip = 0;
    us_init();
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, NULL, &hints, &res) == 0 && res) {
        ip = ntohl(((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr);
        freeaddrinfo(res);
    }
    return ip;
}

void us_ip_str(uint32_t ip, char *out, int n) {
    snprintf(out, (size_t)n, "%u.%u.%u.%u", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
}

static void set_timeouts(SOCK s, int ms) {
#ifdef _WIN32
    DWORD t = (DWORD)ms;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&t, sizeof(t));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&t, sizeof(t));
#else
    struct timeval tv;
    tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

int us_http(uint32_t ip, int port, const char *request, char *out, int cap, int timeoutMs) {
    SOCK s;
    struct sockaddr_in a;
    int len = 0, n;
    us_init();
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == SOCK_BAD) return -1;
    set_timeouts(s, timeoutMs);
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(ip);
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) != 0) { CLOSESOCK(s); return -1; }
    if (send(s, request, (int)strlen(request), 0) <= 0) { CLOSESOCK(s); return -1; }
    while (len < cap - 1 && (n = (int)recv(s, out + len, cap - 1 - len, 0)) > 0) len += n;
    out[len] = 0;
    CLOSESOCK(s);
    return len;
}

int us_ssdp_search(char *out, int cap, int timeoutMs) {
    static const char *msg =
        "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";
    SOCK s;
    struct sockaddr_in a;
    int len = 0, n, k;
    us_init();
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == SOCK_BAD) return 0;
    set_timeouts(s, 400);
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(1900);
    a.sin_addr.s_addr = inet_addr("239.255.255.250");
    for (k = 0; k < 2; k++) sendto(s, msg, (int)strlen(msg), 0, (struct sockaddr *)&a, sizeof(a));
    for (k = 0; k < timeoutMs / 400 && len < cap - 1; k++) {
        n = (int)recv(s, out + len, cap - 1 - len, 0);
        if (n > 0) len += n;
    }
    out[len] = 0;
    CLOSESOCK(s);
    return len;
}
