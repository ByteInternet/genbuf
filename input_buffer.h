#ifndef GENCACHE_INPUT_BUFFER_H
#define GENCACHE_INPUT_BUFFER_H

struct input_buffer {
	char *start;   /* Points to the beginning of the buffer            */
	char *current; /* Current position to write in the buffer          */
	char *border;  /* Internal offset of the buffer                    */
	char *end;     /* Points to the end of the buffer                  */
	int available; /* Number of bytes unused in buffer                 */
	int write;     /* Number of bytes that can be written at 'current' */
};

extern struct input_buffer *input_buffer_create (int size);

extern void  input_buffer_update    (struct input_buffer *buffer, int read);
extern  int  input_buffer_find      (struct input_buffer *buffer, int c);
extern  int  input_buffer_validate  (struct input_buffer *buffer);
extern  int  input_buffer_purgeline (struct input_buffer *buffer);
extern char *input_buffer_getline   (struct input_buffer *buffer);

extern void  input_buffer_free      (struct input_buffer *buffer);

#endif /* GENCACHE_INPUT_BUFFER_H */
