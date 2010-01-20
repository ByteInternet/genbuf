#include "defines.h"
#include "net_tools.h"

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "log.h"

static int parse_resource_spec(char **ipaddr, int *port, char *res)
{
	char *offset;

	/* Check if it's a colon seperated resource (<port> vs. <ip>:<port>)*/
	offset = rindex(res, ':');
	if (offset != NULL)
	{
		/* Got <ip>:<port> combo, let's process */
		*ipaddr = strndup(res, offset - res);

		/* Leave res pointer after the ':' */
		res = offset + 1;
	}

	/* Parse port number */
	*port = strtol(res, &offset, 10);
	
	/* Return if we parsed all information (if there was garbage after number parsing) */
	return offset != res + strlen(res);
}

int net_get_socketaddr (struct sockaddr_in *addr, char *res)
{
	int port;
	char *ipaddr = NULL;
	struct hostent *lookup;

	/* Parse resource data */
	if (parse_resource_spec(&ipaddr, &port, res) != 0)
	{
		Log2(error, res, "Resource string invalid format");
		return FALSE;
	}

	/* Setup the listening socket address */
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	if (ipaddr != NULL)
	{
		lookup = gethostbyname2(ipaddr, AF_INET);
		if (lookup == NULL)
		{
			Log2(error, hstrerror(h_errno), "On input ip lookup");
		}
		if (lookup->h_addr_list[0] == NULL || lookup->h_length != 4)
		{
			SysErr(errno, "On resolving ip-address");
			addr->sin_addr.s_addr = INADDR_ANY;
			return FALSE;
		}
		addr->sin_addr.s_addr = ((u_int32_t*) lookup->h_addr_list[0])[0];

		free(ipaddr);
	}
	else
	{
		addr->sin_addr.s_addr = INADDR_ANY;
	}


	return TRUE;
}

int net_get_protocol (char *proto)
{
	struct protoent *protent;
	
	/* Lookup the protocol */
	protent = getprotobyname(proto);
	Fatal(protent == NULL, hstrerror(h_errno), "On protocol lookup: %s.\n");

	return protent->p_proto;
}

int net_create_listening_socket (char *res, char *proto, int protocol)
{
	struct sockaddr_in addr;
	int fd, sockop;
	
	Require(net_get_socketaddr(&addr, res));
	fd = net_create_socket(protocol, proto);
	
	SysFatal(bind(fd, &addr, sizeof(struct sockaddr_in)) == -1, errno, "On socket bind");

	/* If reading the flags failed, return error indication now. */
	SysFatal((sockop = fcntl(fd, F_GETFL, 0)) == -1, errno, "On getting filedescriptor control options");
	/* Store modified flag word in the descriptor. */
	SysFatal(fcntl(fd, F_SETFL, sockop | O_NONBLOCK) == -1, errno, "On setting filedescriptor O_NONBLOCK control option");

	return fd;
}

int net_create_socket (int protocol, char *proto)
{
	int socktype = 0;
	int fd, sockop;
	
	/* Determine socktype */
	if (strcmp(proto, "tcp") == 0)
		socktype = SOCK_STREAM;
	else if (strcmp(proto, "udp") == 0)
		socktype = SOCK_DGRAM;
	else
		Fatal(TRUE, "Unkown internal usage of protocol specification", "IMPOSSIBLE");
	
	/* Create socket */
	SysFatal((fd = socket(PF_INET, socktype, protocol)) == -1, errno, "On socket create");

	/* Set SO_KEEPALIVE and SO_REUSEADDR on socket */
	sockop = 1;
	SysFatal(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &sockop, sizeof(sockop)) == -1, errno, "On setting socket option SO_KEEPALIVE");
	SysFatal(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sockop, sizeof(sockop)) == -1, errno, "On setting socket option SO_REUSEADDR");

	return fd;
}
