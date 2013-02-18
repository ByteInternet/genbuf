#include "defines.h"
#include "logger.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "output.h"
#include "buffer.h"
#include "log.h"

static void logger_set_destination (struct logger *this, struct output_handler *handler)
{
	/* Basic assertions */
	Require(
		this    != NULL &&
		handler != NULL
	);

	/* There shall be only one.. */
	Fatal(this->dest != NULL, "Destination already set", "Logger");

	this->dest = handler;
}

static int logger_write_backlog (struct logger *this, FILE *backlog_out)
{
	int i, qsize;
	char *msg;
	
	/* Basic assertions */
	Require (
		this != NULL &&
		backlog_out != NULL
	);

	/* Continue filling the backlog with queued messages */
	qsize = this->buffer->size(this->buffer);
	for (i = 0; i < qsize; i++)
	{
		/* Fetch message from buffer */
		msg = this->buffer->pop(this->buffer);

		/* Check for buffer end */
		if (msg == NULL)
		{
			/* Reader has already stopped, we should stop
			 * and continue another lifetime */
			fclose (backlog_out);
			return -2;
		}

		/* Write message to backlog */
		if (fputs(msg, backlog_out) == EOF)
		{
			SysErr(errno, "While trying to write to backlog");
			this->buffer->unpop(this->buffer, msg);

			/* Error on backlog_out */
			return -1;
		}

		fflush(backlog_out);
		free(msg);
		msg = NULL;
	}
	
	return 0;
}

static void logger_run (struct logger *this)
{
	size_t msglen;
	char *msg;
	FILE *backlog_in, *backlog_out;
	
	/* Basic assertions */
	Require(this != NULL);

	msg = NULL;
	backlog_in  = NULL;
	backlog_out = NULL;

	/* Check if we've got a destination to log to */
	Fatal(this->dest == NULL, "No destination set", "Logger");

	/* Try to open an old backlog file */
	if ((backlog_in = fopen(this->backlog_file, "r")) != NULL)
	{
		if (getline(&msg, &msglen, backlog_in) == -1)
		{
			SysErr(errno, "While trying to read from old backlog");
			fclose(backlog_in);
			backlog_in = NULL;
			if (msg != NULL)
			{
				free(msg);
				msg = NULL;
			}
		}
	}
	else
	{
		Log2(info, "No initial backlog", "Logger");
	}
	
	/* If there's no old message from backlog, wait for first one from the buffer */
	if (msg == NULL)
	{
		/* Retrieve the first message */
		msg = this->buffer->pop(this->buffer);
	}

	/* While we've got a message to send */
	while (msg)
	{
		/* Check for error state in destination */
		if (this->dest->state == os_error)
		{
			Log2(error, "Destination in insane state, shutting down", "Logger");
			exit(1);
		}
	
		/* Try delivering the message */	
		if (deliver_message(this->dest, msg))
		{
			/* Message was sent */
			free(msg);
			msg = NULL;
			msglen = 0;

			if (backlog_in)
			{
				/* There is a backlog */
				if (getline(&msg, &msglen, backlog_in) == -1)
				{
					/* EOF reached, no more backlog */

					/* Close backlog */
					if (backlog_in)
						fclose(backlog_in);
					if (backlog_out)
						fclose(backlog_out);
					
					/* Truncate backlog file */
					if (truncate(this->backlog_file, 0))
					{
						SysErr(errno, "While trying to truncate the backlog file");
					}
						
					/* Reset variables */
					backlog_in  = NULL;
					backlog_out = NULL;
					
					/* Pop a message of the queue*/
					msg = this->buffer->pop(this->buffer);
				}
				else
				{
					/* Succesfully read a message from the backlog */

					/* If backlog is still opened */
					if (backlog_out)
					{
						/* Seems like all is fine, stop writing to backlog file */
						fclose(backlog_out);
						backlog_out = NULL;
					}

				}
			}
			else
			{
				/* There's no backlog, so just take the next message of the queue */
				msg = this->buffer->pop(this->buffer);
			}
		}
		else
		{
			/* Message was not sent */

			if (backlog_in)
			{
				/* We've already got a backlog */

				/* If backlog_out is not opened try opening it */
				if (! backlog_out)
				{
					if (this->backlog_file == NULL)
					{
						/* No backlog specified */
						continue;
					}
					else if ((backlog_out = fopen(this->backlog_file, "a")) != NULL)
					{
						/* Open of backlog failed, there's nothing we can do but.. */
						SysErr(errno, "While trying to open backlog file for appending");
						continue;
					}
				}

				switch (logger_write_backlog(this, backlog_out))
				{
					case -1:
						/* Write failed */
						if (ferror(backlog_out))
						{
							fclose(backlog_out);
							backlog_out = NULL;
						}
						break;

					case -2:
						/* End of buffer reached */
						fclose(backlog_out);
						fclose(backlog_in);
						return;

					default:
						/* All went well, continue */
						break;
				}

				/* Retry delivery */
			}
			else
			{
				/* There is no backlog yet */

				/* Open backlog */
				if (!this->backlog_file)
				{
					/* No backlog specified */
					continue;
				}

				backlog_out = fopen(this->backlog_file, "a");

				if ((backlog_in = fopen(this->backlog_file, "r")) == NULL)
				{
					SysErr(errno, "An error occured when trying to open the backlog file for reading");
				}

				if (backlog_out == NULL)
				{
					SysErr(errno, "An error occured when trying to open the backlog file for writing");
					continue;
				}

				/* Write current message to backlog (and free it) */
				if (fputs(msg, backlog_out) == EOF)
				{
					SysErr(errno, "While trying to write to backlog");
					this->buffer->unpop(this->buffer, msg);

					/* Error on backlog_out */
					continue;
				}
				fflush(backlog_out);
				free(msg);
				msg = NULL;
				msglen = 0;

				/* Write current queue to backlog */
				switch (logger_write_backlog(this, backlog_out))
				{
					case -1:
						/* Write failed */
						if (ferror(backlog_out))
						{
							fclose(backlog_out);
							backlog_out = NULL;
						}
						break;

					case -2:
						/* End of buffer reached */
						fclose(backlog_out);
						fclose(backlog_in);
						return;

					default:
						/* All went well, continue */
						break;
				}

				/* Read first message from backlog (also sets file position) */
				if(getline(&msg, &msglen, backlog_in) == -1)
				{
					SysErr(errno, "While reading from just created backlog file");
					if (ferror(backlog_in))
					{
						fclose(backlog_in);
						backlog_in = NULL;
					}
				}

				/* Resume delivery tries from the backlog */
			}
		}
	}

	/* We're done */
}

static void logger_cleanup (struct logger *this)
{
	/* Basic assertions */
	Require(this != NULL);

	/* Cleanup the output_handler */
	if (this->dest != NULL)
		this->dest->cleanup(this->dest);
	
	/* Free structure */
	free(this);
}

struct logger *logger_init(struct buffer *buffer)
{
	/* Basic assertions */
	Require(buffer != NULL);
	
	/* Create logger structure */
	struct logger *this = (struct logger*) malloc (sizeof(struct logger));
	SysFatal(this == NULL, errno, "On logger structure creation");

	/* Initialize variables */
	this->backlog_file = NULL;
	this->dest   = NULL;
	this->buffer = buffer;
	
	/* Set the handler functions */
	this->set_destination = logger_set_destination;
	this->run             = logger_run;
	this->cleanup         = logger_cleanup;
	
	/* Return */
	return this;
}
