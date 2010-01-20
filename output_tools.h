#ifndef OUTPUT_TOOLS_H
#define OUTPUT_TOOLS_H

#include "output.h"

extern int output_handler_common_write   (struct output_handler*, char *msg, int msglen, int *done);
extern int output_handler_common_cleanup (struct output_handler *);
extern int output_handler_common_nothing (struct output_handler*);

extern struct output_handler *output_handler_common_init  (char *type, char *res, int fd);

#endif /* OUTPUT_TOOLS_H */
