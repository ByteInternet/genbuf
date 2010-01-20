#ifndef GENCACHE_SETUP_H
#define GENCACHE_SETUP_H

#include "types.h"
#include "input.h"
#include "output.h"

extern struct  input_handler  *create_input_handler (enum io_types type, char *res);
extern struct output_handler *create_output_handler (enum io_types type, char *res);

#endif /* GENCACHE_SETUP_H */
