/* Kleine, draagbare netwerklaag: UDP (niet-blokkerend) en een simpele HTTP-over-TCP voor UPnP.
 * Werkt op iOS/Linux (BSD-sockets) en Windows (Winsock). Adressen als IPv4 in host-volgorde. */
#ifndef SGB_NETSOCK_H
#define SGB_NETSOCK_H

#include <stdint.h>

int us_init(void);
/* UDP-socket op poort (0 = willekeurig). broadcast: mag naar 255.255.255.255 sturen. Geeft handle >= 0. */
int us_open(int port, int broadcast);
void us_close(int s);
int us_send(int s, uint32_t ip, int port, const void *data, int len);
/* 0 = niets, >0 = aantal bytes; ip/port van de afzender */
int us_recv(int s, void *buf, int cap, uint32_t *ip, int *port);
int us_bound_port(int s);
uint32_t us_local_ip(void);                       /* IPv4 van deze machine in het LAN (of 0) */
uint32_t us_resolve(const char *host);            /* blokkerend DNS; 0 = fout */
void us_ip_str(uint32_t ip, char *out, int n);
/* blokkerende HTTP via TCP (alleen voor UPnP in het LAN). Geeft lengte antwoord (incl. headers) of -1. */
int us_http(uint32_t ip, int port, const char *request, char *out, int cap, int timeoutMs);
/* multicast-zoekbericht (SSDP) en antwoorden verzamelen, blokkerend tot timeoutMs */
int us_ssdp_search(char *out, int cap, int timeoutMs);

#endif
