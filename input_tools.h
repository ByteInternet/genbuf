#ifndef GENCACHE_INPUT_TOOLS_H
#define GENCACHE_INPUT_TOOLS_H

#include "input.h"
#include "input_buffer.h"

/* Special states we recognize */
enum input_state {
	is_ready,
	is_err,
	is_eof
};

/* Input handler private data */
#define	DATA	((struct ih_common_priv*) this->priv)
struct ih_common_priv {
                 int  fd;
 struct input_buffer *inbuf;
    enum input_state  state;
};

/* Tooling functions */
extern int
	input_handler_common_getfd(struct input_handler*);
extern int
	input_handler_common_cleanup(struct input_handler*);
extern int
	input_handler_common_read(struct input_handler*, struct reader*);
extern struct input_handler *
	input_handler_common_init(char *type, char *res, int fd);
	
#endif /* GENCACHE_INPUT_TOOLS_H */
