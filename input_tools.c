#include "defines.h"
#include "input_tools.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/ioctl.h>

#include "input_buffer.h"
#include "log.h"

int input_handler_common_read (struct input_handler *this, struct reader *report)
{
	int err, readcount, maxread;
	char *msg;

	Log2(debug, "Read requested", "[input_tools.c]{read}");

	/* Check if this input source is in a valid state */
	if (DATA->state == is_eof)
	{
		Log2(debug, "Already at EOF", "[input_tools.c]{read}");
		return -1;
	}

	/* Read data */
	Require (DATA->inbuf->write > 0);
	Require (DATA->inbuf->available > 0);

	/* Determine how much data is available */
	if (ioctl(DATA->fd, FIONREAD, &maxread) == -1)
	{
		/* Error while checking FIONREAD on filedescriptor */
		err = errno;
		SysErr(err, "Error on ioctl FIONREAD on input source");
		maxread = INT_MAX;
		DATA->state = is_eof;
		return -1;
	}

	/* If no data is available */
	if (maxread == 0)
	{
		Log2(debug, "No data available", "[input_tools.c]{read}");
		DATA->state = is_eof;
		return -1;
	}
	maxread = MIN(DATA->inbuf->write, maxread);
	
	readcount = read(DATA->fd, DATA->inbuf->current, maxread);
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
					Log2(debug, "Recovered", "[input_tools.c]{read}");
					return 0;
					
				default:
					/* Fatal */
					Log2(debug, "Input is dead", "[input_tools.c]{read}");
					DATA->state = is_eof;
					return -1;
			}

		case 0:
			/* '0' Bytes read, EOF? */
			switch(err)
			{
				case EAGAIN:
				case EINTR:
					/* Recoverable */
//
// Bug found:
// We should have read something, or do it now, or just wait for a reconnect...
// Furthermore if it really was recoverable, we would have received a -1!!!!
//
// Log2(debug, "Recovered from '0' bytes read from input source", "[input_tools.c]{read}");
// return 0;
//
					
				default:
					/* Fatal */
					SysErr(err, "[input_tools.c]{read}: Read '0' bytes from input source");
					DATA->state = is_eof;
					return -1;
			}

		default:
			/* Successfull read */
			Log2(debug, "Succesfully read data", "[input_tools.c]{read}");
			input_buffer_update(DATA->inbuf, readcount);
	}

	/* Check if we're in an error state */
	if (DATA->state == is_err)
	{
		/* Purge state */
		if (! input_buffer_purgeline(DATA->inbuf))
		{
			Log2(debug, "Purging is not done", "[input_tools.c]{read}");
			DATA->state = is_err;
			return 0;
		}
		
		/* We purged the remainder of the oversized line, continue reading */
		DATA->state = is_ready;
	}
	
	/* Report all complete read lines */
	while ((msg = input_buffer_getline(DATA->inbuf)) != NULL)
	{
		/* Successfully read a line */
		Log2(debug, msg, "[input_tools.c]{read} data");
		report->report_data(report, msg);
	}

	/* Check if there's no buffer space left */
	if (DATA->inbuf->available == 0)
	{
		Log2(debug, "Buffer is full and no line is available, start purging", "[input_tools.c]{read}");
		DATA->state = is_err;
		Fatal(input_buffer_purgeline(DATA->inbuf), "I should have read a line", "input_tools_common_read->purgeline(2)");
	}

	Log2(debug, "Read done", "[input_tools.c]{read}");
	return 0;
}

int input_handler_common_getfd (struct input_handler *this)
{
	/* Return the filedescriptor */
	return DATA->fd;
}

int input_handler_common_cleanup (struct input_handler *this)
{
	if (this)
	{
		if (DATA)
		{
			input_buffer_free(DATA->inbuf);
		
			/* Close the filedescriptor */
			if (close(DATA->fd) == -1)
			{
				SysErr(errno, "[Common input] On closing file");
				return 0;
			}
		
			free(DATA);
			this->priv = NULL;
		}
		else
		{
			CustomLog(__FILE__, __LINE__, error, "Input buffer was already destroyed?");
		}
		
		free(this);
		this = NULL;
	}
	else
	{
		CustomLog(__FILE__, __LINE__, error, "Trying to cleanup an already cleanup input handler?");
	}

	/* All went well */
	return 1;
}

struct input_handler *input_handler_common_init (char *type, char *res, int fd)
{
	/* Declare and create basic input_handler structure */
	struct input_handler *this =
		(struct input_handler*) malloc (sizeof(struct input_handler));

	/* Check memory allocation */
	Fatal(this == NULL, "Memory allocation failed!", "Error when loading input handler");

	/* Print a checkpoint log message */
	Log2(debug, res, "CP:IH->init");
	
	/* Allocate private data space */
	this->priv = malloc (sizeof(struct ih_common_priv));

	/* Check to see wheter resource specification succeeded */
	SysFatal(this->priv == NULL, errno, "When allocating private data");
	
	/* Initialize fields */
	this->type    = type;
	this->res     = res;
	this->err     = NULL;
	this->read    = input_handler_common_read;
	this->getfd   = input_handler_common_getfd;
	this->cleanup = input_handler_common_cleanup;

	/* Allocate initial msg buffer */
	DATA->inbuf = input_buffer_create(GENCACHE_MAX_MSG_SIZE);

	/* Register FD */
	DATA->fd    = fd;
	DATA->state = is_ready;

	return this;
}
