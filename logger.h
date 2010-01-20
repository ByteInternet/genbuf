#ifndef GENCACHE_LOGGER_H
#define GENCACHE_LOGGER_H

#include "output.h"
#include "buffer.h"

struct logger {
	char * backlog_file;

	struct output_handler *dest;
	struct buffer         *buffer;

	void (*set_destination) (struct logger*, struct output_handler*);
	void (*run)             (struct logger*);

	void (*cleanup) (struct logger*);
};

extern struct logger *logger_init(struct buffer *);

#endif /* GENCACHE_LOGGER_H */
