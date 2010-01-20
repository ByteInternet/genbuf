#include "output_udp.h"

#include <sys/types.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "output_tools.h"
#include "log.h"

static int output_udp_connect (struct output_handler *this)
{
	return 0;
}

static int output_udp_disconnect (struct output_handler *this)
{
	return 0;
}

static int output_udp_write (struct output_handler *this, char *str, int strlen, int*send)
{
	return 0;
}

static int output_udp_cleanup (struct output_handler *this)
{
	return 0;
}

struct output_handler *output_handler_udp_init (char *res)
{
	struct output_handler *retval = output_handler_common_init("file", res, -1);
	
	retval->connect    = output_udp_connect;
	retval->disconnect = output_udp_disconnect;
	retval->write      = output_udp_write;
	retval->cleanup    = output_udp_cleanup;

	return retval;
}
