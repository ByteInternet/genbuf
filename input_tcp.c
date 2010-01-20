#include "defines.h"
#include "input_tcp.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

#include "net_tools.h"
#include "input_tcp_connection.h"
#include "input_tools.h"
#include "log.h"

/* Input handler private data */
#ifdef DATA
#undef DATA
#endif

#define	DATA	((struct ih_tcp_priv*) this->priv)
struct ih_tcp_priv {
    int  fd;
};

static int input_handler_tcp_read (struct input_handler *this, struct reader *report)
{
	struct input_handler *handler;
	int fd, err;
	
	/* Print a checkpoint log message */
	Log(debug, "CP:IH[tcp]->read");

	/* Until noted otherwise, repeat */
	while (TRUE)
	{
		fd = accept(DATA->fd, NULL, NULL);

		/* Check accept's result */
		if (fd != -1)
		{
			/* Got a connection, create a new connection input handler */
//			shutdown(fd, 1);
			handler = input_handler_tcp_connection_init("<slave>", fd);

			Fatal(handler == NULL, "NULL", "[TCP input handler] On connection handler creation");

			/* Report it to the reader */
			report->add_source(report, handler);
		}
		else
		{
			/* Error returned */
			err = errno;

			switch (err)
			{
				case EAGAIN:
					/* Retryable error (eg. would have blocked); Nothing for now */
					return 0;
				default:
					/* Socket died? */
					SysErr(err, "[TCP input handler] While trying to accept connection");
					return -1;
			}
		}
	}

	/* Return succes */
	return 0;
}

static int input_handler_tcp_getfd (struct input_handler *this)
{
	/* Print a checkpoint log message */
	Log(debug, "CP:IH[tcp]->getfd");

	/* Return the tcpdescriptor */
	return DATA->fd;
}

static int input_handler_tcp_cleanup (struct input_handler *this)
{
	/* Print a checkpoint log message */
	Log(debug, "CP:IH[tcp]->cleanup");

	/* Free private data */
	free(this->priv);

	/* Close the tcpdescriptor */
	if (close(DATA->fd) == -1)
	{
		SysErr(errno, "[TCP input handler] On closing tcp");
		return 0;
	}
	
	/* All went well */
	return 1;
}

struct input_handler *input_handler_tcp_init (char *res)
{
	int proto;
	
	/* Declare and create basic input_handler structure */
	struct input_handler *this =
		(struct input_handler*) malloc (sizeof(struct input_handler));

	/* Check memory allocation */
	Fatal(this == NULL, "Memory allocation failed!", "Error when loading input handler");

	/* Print a checkpoint log message */
	Log2(debug, res, "CP:IH[tcp]->init");
	
	/* Allocate private data space */
	this->priv = malloc (sizeof(struct ih_tcp_priv));

	/* Check to see wheter resource specification succeeded */
	SysFatal(this->priv == NULL, errno, "When allocating private data");
	
	/* Initialize fields */
	this->type    = "tcp-server";
	this->res     = res;
	this->err     = NULL;
	this->read    = input_handler_tcp_read;
	this->getfd   = input_handler_tcp_getfd;
	this->cleanup = input_handler_tcp_cleanup;

	/* Open the input stream */
	proto = net_get_protocol("tcp");
	DATA->fd = net_create_listening_socket(res, "tcp", proto);
	SysFatal(listen(DATA->fd, 5) == -1, errno, "[TCP input handler] When trying to listen to socket");

	return this;
}
