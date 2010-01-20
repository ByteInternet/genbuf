#ifndef GENCACHE_INPUT_H
#define GENCACHE_INPUT_H

#include "reader.h"
#include "types.h"

/** Input Handler module interface
 *
 * Params:
 *   res   -> Resource specifier (module dependant)
 *
 * Returns:
 *   int   -> Non-zero on failure
 *
 */
struct input_handler {
	void *priv;
	char *err;
	char *res;
	char *type;
	int   (*read)    (struct input_handler*, struct reader*);            /* read returns: -1 on EOF */
	int   (*getfd)   (struct input_handler*);
	int   (*cleanup) (struct input_handler*);
};

#else
struct input_handler;
#endif /* GENCACHE_INPUT_H */
