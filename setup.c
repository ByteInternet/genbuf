#include "defines.h"
#include "setup.h"

#include <stdlib.h>

#include "input_tcp.h"
#include "input_udp.h"
#include "input_unix.h"
#include "input_file.h"

#include "output_tcp.h"
#include "output_udp.h"
#include "output_unix.h"
#include "output_file.h"

#include "log.h"

struct input_handler*
	create_input_handler (enum io_types type, char *res)
{
	struct input_handler *retval = NULL;
	
	/* Check if valid type is specified */
	Require(type != type_unknown);

	/* Initialize all values for the correct type */
	switch (type)
	{
		case type_file:
			retval = input_handler_file_init(res);
			break;

		case type_unix:
			retval = input_handler_unix_init(res);
			break;

		case type_udp:
			retval = input_handler_udp_init(res);
			break;

		case type_tcp:
			retval = input_handler_tcp_init(res);
			break;

		default:
			Fatal(FALSE, "Unkown io_type value", NULL);
	}

	/* Sanity check the existance of the functions */
	Require (
			  retval          != NULL &&
	          retval->read    != NULL &&
			  retval->getfd   != NULL &&
	          retval->cleanup != NULL
	        );
	
	/* Return input_handler*/
	return retval;
}

struct output_handler*
	create_output_handler (enum io_types type, char *res)
{
	struct output_handler *retval = NULL;
	
	/* Check if valid type is specified */
	Require(type != type_unknown);

	/* Initialize all values for the correct type */
	switch (type)
	{
		case type_file:
			retval = output_handler_file_init(res);
			break;

		case type_unix:
			retval = output_handler_unix_init(res);
			break;

		case type_udp:
			retval = output_handler_udp_init(res);
			break;

		case type_tcp:
			retval = output_handler_tcp_init(res);
			break;

		default:
			Fatal(FALSE, "Unkown io_type value", NULL);
	}

	/* Sanity check the existance of the functions */
	Require (
		retval             != NULL &&
		retval->connect    != NULL &&
		retval->disconnect != NULL &&
		retval->timeout    != NULL &&
		retval->write      != NULL &&
		retval->cleanup    != NULL
	);

	/* Return input_handler*/
	return retval;
}

