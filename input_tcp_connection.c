#include "defines.h"
#include "input_tcp_connection.h"

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
#include "input_tools.h"
#include "log.h"

struct input_handler *input_handler_tcp_connection_init (char *res, int fd)
{
	return input_handler_common_init("tcp-conn", res, fd);
}
