#ifndef GENCACHE_READER_H
#define GENCACHE_READER_H

#include "input.h"
#include "buffer.h"

#include <sys/types.h>

struct reader {
	int handler_count;
	int handlers_alloc;
	
	struct handler_list {
		int  fd;
		struct input_handler *handler;
	} *handlers;

	fd_set fds;
	struct buffer *buffer;

	void (*add_source)  (struct reader*, struct input_handler*);
	void (*run)         (struct reader*);
	void (*report_data) (struct reader*, char*);

	void (*cleanup) (struct reader*);
};

extern struct reader *reader_init();

#else
struct reader;
#endif /* GENCACHE_READER_H */
