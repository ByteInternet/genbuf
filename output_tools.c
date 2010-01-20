#include "defines.h"
#include "output_tools.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "log.h"


int output_handler_common_write (struct output_handler *this, char *str, int strlen, int *todo)
{
	ssize_t sent;
	
	/* Check if we're in a valid state */
	Require(
		this->state == os_ready   ||
		this->state == os_sending
	);

	/* Try to send (what's left of) the message */
	Fatal(str[strlen - 1] != '\n', "No newline at the end of the string", "[output_handler.c]{write}");
	Log2(debug, str + (strlen - *todo), "[output_tools.c]{write} data");
	errno = 0;
	sent = write (this->fd, str + (strlen - *todo), *todo);

	/* Check if anything was sent */
	while (sent > 0)
	{
		/* It appears something was sent */
		*todo -= sent;

		/* Strange sanity check */
		Require (*todo >= 0);

		/* Check if we're done with the message */
		if (*todo == 0)
		{
			/* We're done sending */
			this->state = os_ready;
			return TRUE;
		}

		/* We're not done yet */
		this->state = os_sending;
		sent = write(this->fd, str + (strlen - *todo), *todo);
	}

	/* Check what errno says */
	this->err = errno;
	switch (this->err)
	{
		/* Check if this error is recoverable */
		case 0:
			Log2(warning, "SUCCES?? on write", "[output_tools.c]{write}");
			this->state = os_sending;
			return FALSE;
		case EAGAIN:
			Log2(warning, "EAGAIN on write", "[output_tools.c]{write}");
			this->state = os_sending;
			return FALSE;
		case EPIPE:
			Log2(warning, "EPIPE on write", "[output_tools.c]{write}");
			this->state = os_error;
			return FALSE;
		
		/* Otherwise goto the error state */
		default:
			SysErr(this->err, "[output_tools.c:output_handler_common_write] While trying to write");
			this->state = os_error;
			return FALSE;
	}
}


int output_handler_common_do_nothing (struct output_handler *this)
{
	/* Print a message for logging purposes */
	Log2(warning, "do_nothing() called", this->res);

	/* Return success */
	return TRUE;
}


int output_handler_common_cleanup (struct output_handler *this)
{
	/* Disconnect if needed */
	if (this->state != os_disconnected)
		this->disconnect(this);
	
	/* Free output_handler structure */
	free(this);
	
	/* Return success */
	return TRUE;
}


struct output_handler *output_handler_common_init  (char *type, char *res, int fd)
{
	/* Allocate the structure */
	struct output_handler *this = (struct output_handler*) malloc(sizeof(struct output_handler));

	/* Check if allocation succeeded */
	SysFatal(this == NULL, errno, "While trying to create output handler structure");

	/* Initialize fields */
	this->state     = os_disconnected;

	this->fd        = fd;
	this->err       = 0;
	this->retry     = 3;
	this->res       = res;
	this->type      = type;
	this->priv      = NULL;
	
	this->connect    = NULL;
	this->disconnect = NULL;
	this->timeout    = output_handler_common_do_nothing;
	this->write      = output_handler_common_write;
	this->cleanup    = output_handler_common_cleanup;

	return this;
}

