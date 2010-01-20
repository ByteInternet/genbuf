#include "defines.h"
#include "reader.h"

#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include "buffer.h"
#include "log.h"

#define HANDLERS_STEPPING	8

static void reader_add_source (struct reader *this, struct input_handler *handler)
{
	int oldstate;
	void *tmp;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

	if (this->handlers_alloc % HANDLERS_STEPPING == 0)
	{
		tmp = realloc(this->handlers, (this->handler_count + HANDLERS_STEPPING + 1) * sizeof(struct handler_list));

		if (tmp != NULL)
		{
			this->handlers = (struct handler_list*) tmp;
			
			this->handlers[this->handler_count].handler = handler;
			this->handlers[this->handler_count].fd      = handler->getfd(handler);

			CustomLog(__FILE__, __LINE__, error, "add_source(handler=%p, [type=%s, res=%s, fd=%d])", handler, handler->type, handler->res, this->handlers[this->handler_count].fd);

			Require(this->handlers[this->handler_count].fd != -1);

			this->handler_count++;
		}
		else
		{
			SysErr(errno, "[Reader] Realloc for more handlers failed");
			Log2(warning, "Memory reallocation failed", "Input handler ignored");
			handler->cleanup(handler);
		}
	}

	pthread_setcancelstate(oldstate, NULL);

	pthread_testcancel();
}

static void remove_handler (struct reader *this, int index)
{
	int oldstate;
	struct input_handler *handler;
	
	Require(index >= 0 && index < this->handler_count);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

	FD_CLR(this->handlers[index].fd, &this->fds);
	handler = this->handlers[index].handler;
	
	CustomLog(__FILE__, __LINE__, error, "remove_source(handler=%p, [type=%s, res=%s, fd=%d])", handler, handler->type, handler->res, this->handlers[index].fd);

	if (index < this->handler_count - 1)
	{
		/* If not move the rest one place to the front */
		memmove(
			this->handlers + index,
			this->handlers + (index + 1),
			sizeof(struct handler_list) * (this->handler_count - index - 1)
		);
	}

	/* Make the handler tidy up */
	handler->cleanup(handler);
	
	/* One handler less to worry about */
	this->handler_count--;
	
	pthread_setcancelstate(oldstate, NULL);

	pthread_testcancel();
}

static void reader_run (struct reader *this)
{
	int i, n, fd, count, active, status;

rerun:
	do
	{
		active = 0;
		n = 0;
	
		/* Cycle through the handlers to see who's responisble */
		count = this->handler_count;
		for (i = 0; i < count; i++)
		{
			if (FD_ISSET(this->handlers[i].fd, &this->fds))
			{
				struct input_handler *handler = this->handlers[i].handler;
				
				/* Got message, push it on the queue */
				Log(debug, "I'm trying to cope with something here");
				status = handler->read(handler, this);
				
				switch (status)
				{
					case -1:
						Log(debug, "Error in input_handler, removing it!");
						remove_handler(this, i);
						goto rerun;
					case 0:
						/* Nothing special to report */
						Log(debug, "I think I coped with that quite nicely");
						break;
					default:
						Log(debug, "Unknown error from input_handler!");
						fprintf(stderr, "Handler returned: %d!\n", status);
						remove_handler(this, i);
						goto rerun;
				}
			}
		}

		/* Re-activate fd's for select */
		FD_ZERO(&this->fds);
		count = this->handler_count;
		for (i = 0; i < count; i++)
		{
			fd = this->handlers[i].fd;
			FD_SET(fd, &this->fds);
			n = MAX(fd, n);
		}
		
		/* Check if there is anything left listening for */
		if (this->handler_count == 0)
			return;

		Log(debug, "Calling select()");
	}
	while ((active = select(n+1, &this->fds, NULL, NULL, NULL)) != -1);

	if (errno == EBADF)
	{
		count = this->handler_count;
		errno = 0;
		Log(error, "Got bad filedescriptor, trying recover!");
		for (i = 0; i < count; i++)
		{
			fd = this->handlers[i].fd;
			if (fcntl(fd, F_GETFL) == -1)
			{
				if (errno == EBADF)
				{
					Log(error, "Recover succeeded, found bad filedescriptor!");
				}
				else
				{
					Log(error, "Bad filedescriptor expected, found something else, does this do the trick?");
				}
				
				remove_handler(this, i);
				goto rerun;
			}
		}
	}
	
	SysErr(errno, "Select failed");
}

static void reader_report_data (struct reader *this, char *data)
{
	Require(data != NULL);
	
	this->buffer->push(this->buffer, data);
}

static void reader_cleanup (struct reader *this)
{
	int i;
	
	/* Cleanup the handlers */
	for (i = 0; i < this->handler_count; i++)
	{
		this->handlers[i].handler->cleanup(this->handlers[i].handler);
	}

	/* Add the finished symbol to the buffer */
	this->buffer->push(this->buffer, NULL);

	/* Cleanup the handlers_list */
	if (this->handlers != NULL)
		free(this->handlers);
	
	/* Cleanup ourselves */
	free(this);
}

struct reader *reader_init(struct buffer *buffer)
{
	struct reader *retval = (struct reader*) malloc (sizeof(struct reader));

	SysFatal (retval == NULL, errno, "On reader structure allocation");
		
	retval->handler_count  = 0;
	retval->handlers_alloc = 0;
	retval->handlers       = NULL;
	retval->buffer         = buffer;

	FD_ZERO(&retval->fds);

	retval->add_source  = reader_add_source;
	retval->run         = reader_run;
	retval->report_data = reader_report_data;
	retval->cleanup     = reader_cleanup;

	return retval;
}

