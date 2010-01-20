#include "defines.h"
#include "input_udp.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

#include "input_tools.h"
#include "net_tools.h"
#include "log.h"

int input_handler_udp_read (struct input_handler *this, struct reader *report)
{
	int err, readcount;
	char *msg;

	Log2(debug, "Read requested", "[input_udp.c]{read}");

	/* Check if this input source is in a valid state */
	if (DATA->state == is_eof)
	{
		Log2(debug, "Already at EOF", "[input_udp.c]{read}");
		return -1;
	}

	/* Read data */
	Require (DATA->inbuf->current == DATA->inbuf->start);
	Require (DATA->inbuf->write > 0);
	Require (DATA->inbuf->available > 0);

	readcount = recv(DATA->fd, DATA->inbuf->current, DATA->inbuf->write, MSG_TRUNC);
	err = errno;

	switch (readcount)
	{
		case -1:
			SysErr(err, "Error occured during read from input source");
			/* Error */
			switch(err)
			{
				case EAGAIN:
				case EINTR:
					/* Recoverable */
					Log2(debug, "Recovered", "[input_udp.c]{read}");
					return 0;
					
				default:
					/* Fatal */
					Log2(debug, "Input is dead", "[input_udp.c]{read}");
					DATA->state = is_eof;
					return -1;
			}

		case 0:
			/* End of file */
			SysErr(err, "[input_udp.c]{read}: Read '0' bytes from input source");
			DATA->state = is_eof;
			return -1;

		default:
			/* Successfull read */
			if (readcount > DATA->inbuf->write)
			{
				Log2(err, "Message truncated", "[input_udp.c]{read}");
				return 0;
			}
			
			Log2(debug, "Succesfully read data", "[input_udp.c]{read}");
	}

	/* Message should not contain '\0' */
	void *zero = memchr(DATA->inbuf->current, '\0', readcount);
	if (zero != NULL && zero - (void*) DATA->inbuf->current < readcount)
	{
		/* Check if all trailing characters are '\0' */
		int i;
		for (i = zero - (void*) DATA->inbuf->current; i < readcount; i++)
		{
			if (DATA->inbuf->current[i] != '\0')
			{
				/* Nope it was a zero in the middle of the message */
				Log2(warning, "Message contains zero characters, purged", "[input_udp.c]{read}");
				return 0;
			}
		}
		
		Log2(warning, "Message has trailing zero characters, queued", "[input_udp.c]{read}");
		readcount = zero - (void*) DATA->inbuf->current;
	}

	/* Push the whole msg in the queue */
	msg = (char*) malloc (readcount + 2);
	if (msg == NULL)
	{
		Log2(err, "Unable to allocate memory for message", "[input_udp.c]{read}");
		return 0;
	}

	/* Copy data from buffer in msg */
	memcpy(msg, DATA->inbuf->current, readcount);

	/* Make sure the trailer of the msg is according to genbuf spec  (\n\0) */
	switch (msg[readcount - 1])
	{
		case '\0':
			if (msg[readcount - 2] == '\n')
				break;

			msg[readcount - 1] = '\n';

		case '\n':
			msg[readcount] = '\0';
			break;

		default:
			msg[readcount] = '\n';
			msg[readcount + 1] = '\0';
	}

	/* Queue message */
	report->report_data(report, msg);
	
	/* Success */
	Log2(debug, "Read done", "[input_udp.c]{read}");
	return 0;
}

struct input_handler *input_handler_udp_init (char *res)
{
	int fd, proto;
	
	/* Open the input stream */
	proto = net_get_protocol ("udp");
	fd    = net_create_listening_socket(res, "udp", proto);

	struct input_handler *this = input_handler_common_init("udp", res, fd);

	/* Use oure custom read function */
	this->read = input_handler_udp_read;

	return this;
}
