#ifndef GENCACHE_NET_TOOLS_H
#define GENCACHE_NET_TOOLS_H

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

extern int net_get_socketaddr (struct sockaddr_in *addr, char *res);
extern int net_create_listening_socket (char *res, char *proto, int protocol);
extern int net_get_protocol (char *proto);
extern int net_create_socket (int protocol, char *proto);

#endif /* GENCACHE_NET_TOOLS_H */
