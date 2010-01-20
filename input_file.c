#include "defines.h"
#include "input_file.h"

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
#include "log.h"

struct input_handler *input_handler_file_init (char *res)
{
	int fd;
	
	/* Open the input stream */
	if (strcmp(res, "-") == 0)
	{
		/* This means STDIN */
		fd = 0;
	}
	else
	{
		/* Resource specifier should be a file, so try opening it */
		//fd = open(res, O_RDONLY|O_NONBLOCK);
		fd = open(res, O_RDONLY);
	
		/* Check to see if open failed */
		SysFatal(fd == -1, errno, "[File input handler] On opening file");
	}

	return input_handler_common_init("file", res, 0);
}
