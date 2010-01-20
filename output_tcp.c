#include "defines.h"
#include "output_tcp.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "net_tools.h"
#include "output_tools.h"
#include "log.h"

#define PRIVATE ((struct priv*) this->priv)
struct priv {
	int proto;
};
	
static int output_tcp_connect (struct output_handler *this)
{
	struct sockaddr_in addr;

	Require(
		this != NULL &&
		(
			(this->state == os_disconnected && this->fd == -1) ||
			(this->state == os_connecting && this->fd >= 0)
		)
	);

	/* If initial connect attempt, create socket */
	if (this->state == os_disconnected)
	{
		Log2(info, "Creating new socket", "[TCP output handler]");
		this->fd = net_create_socket (PRIVATE->proto, this->type);
	}
	
	/* Resolve address */
	if (!net_get_socketaddr(&addr, this->res))
	{
		this->err = errno;
		SysErr(this->err, "While trying to make outgoing tcp connection");
		this->state = os_error;
		return FALSE;
	}

	/* Try to connect */
	if (connect(this->fd, &addr, sizeof(struct sockaddr_in)) == -1)
	{
		this->err = errno;
		switch(this->err)
		{
			/* Connection is established */
			case EISCONN:
//				shutdown(this->fd, 0);
				this->state = os_ready;
				return TRUE;
			
			/* Temporary failure */
			case EAGAIN:
			case EINPROGRESS:
			case EALREADY:
				this->state = os_connecting;
				return TRUE;
			
			/* Unknown failure */
			default:
				SysErr(this->err, "While trying to establish outgoing tcp connection");
				this->state = os_error;
				return FALSE;
		}
	}

	/* All went well, we're connected */
	this->state = os_ready;
	return TRUE;
}

static int output_tcp_disconnect (struct output_handler *this)
{
	Require(
		this != NULL &&
		(
			this->state == os_ready			||
			this->state == os_connecting	||
			this->state == os_error			
		)
	);

	this->state = os_disconnected;
	if (close(this->fd))
	{
		this->fd = -1;
		this->err = errno;
		SysErr(this->err, "While closing outgoing tcp connection");
		return FALSE;
	}
	
	this->fd = -1;
	return TRUE;
}

static int output_tcp_timeout (struct output_handler *this)
{
	/* TCP sockets should try to reconnect on timeout */
	this->disconnect(this);

	return FALSE;
}

struct output_handler *output_handler_tcp_init (char *res)
{
	struct output_handler *this = output_handler_common_init("tcp", res, -1);
	
	/* Allocate private data part */
	struct priv *private = (struct priv*) malloc (sizeof(struct priv));
	SysFatal(private == NULL, errno, "While creating TCP output private data");
	this->priv       = private;

	PRIVATE->proto = net_get_protocol(this->type);

	this->connect    = output_tcp_connect;
	this->disconnect = output_tcp_disconnect;
	this->timeout    = output_tcp_timeout;

	return this;
}
