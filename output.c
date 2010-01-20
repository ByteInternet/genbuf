#include "output.h"

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "log.h"

int deliver_message (struct output_handler *handler, char *msg)
{
	struct timeval timeout;
	int todo, retry, msglen, s;
	fd_set fds;

	/* Initialize variables */
	FD_ZERO(&fds);
	todo = msglen = strlen(msg);
	retry = handler->retry;

	/* Try to send the message with reasonable effort */
	while (retry > 0)
	{
		/* If not connected, connect */
		if (handler->state == os_disconnected)
		{
			handler->connect(handler);
		}

		/* If valid filedescriptor, listen for signals */
		if (handler->fd != -1)
		{
			/* Add the handler's fd to the watchlist */
			FD_SET(handler->fd, &fds);
		}
		
		/* Set maximum timeout */
		timeout.tv_sec  = 30;
		timeout.tv_usec = 0;

		/* Perform select */
		if (handler->state == os_ready || handler->state == os_sending)
			s = select(handler->fd + 1, NULL, &fds, NULL, &timeout);
		else
		{
			timeout.tv_sec = 5;
			s = select(0, NULL, NULL, NULL, &timeout);
		}
		
		/* Determine select status */
		switch(s)
		{
			/* Error occured */
			case -1:
				SysErr(errno, "While doing select for message sending");
				return 0;

			/* Timeout occured */
			case 0:
				Log2(warning, "Output handling timeout", "[output.c]{deliver_message}");
				retry--;
				break;

			/* Activity on filedescriptor */
			default:
				Log2(warning, "Output is active", "[output.c]{deliver_message}");
				switch (handler->state)
				{
					/* Try to connect */
					case os_disconnected:
						CustomLog(__FILE__, __LINE__, warning, "Trying to connect output channel!");
						handler->connect(handler);
						break;
						
					/* Check if connection is established */
					case os_connecting:
						CustomLog(__FILE__, __LINE__, warning, "Still connecting output channel!");
						handler->connect(handler);
						break;
					
					/* Try to send the message */
					case os_ready:
					case os_sending:
						retry = 3;
						CustomLog(__FILE__, __LINE__, warning, "Trying to send message(msg=%p, msglen=%d, cont=%p)!", msg, msglen, todo);
						if (FD_ISSET(handler->fd, &fds) && handler->write(handler, msg, msglen, &todo))
						{
							/* Write succesfully completed */
							return 1;
						}
						break;
						
					/* Do a reset */
					case os_error:
						CustomLog(__FILE__, __LINE__, warning, "Resetting output channel!");
						break;
						
					/* Impossible */
					default:
						Log2(impossible, "IMPOSSIBLE state", "[output.c]{deliver_message}");
						return 0;
				}

				/* Continue */
				break;
		}
		
		/* If we're in error state, disconnect */
		if (handler->state == os_error)
		{
			/* SysErr(handler->err, "[output.c]{deliver_message} Handler is in an error state"); */
			handler->disconnect(handler);
			retry --;
		}
	}

	/* Out of retry's */
	Log(error, "Maximum amount of retry's reached");
	return 0;
}
