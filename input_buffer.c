#include "defines.h"
#include "input_buffer.h"

#ifndef NDEBUG
#define NDEBUG
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "log.h"

#undef fprintf
#define fprintf if(0)fprintf

struct input_buffer *input_buffer_create (int size)
{
	struct input_buffer *buffer;

	Log2(debug, "Creating buffer", "[input_buffer.c]{create}");
	
	/* Claim memory for input buffer */
	buffer = (struct input_buffer*) malloc (sizeof(struct input_buffer));
	SysFatal(buffer == NULL, errno, "While trying to create input buffer structure");
	buffer->start = (char*) malloc (size);
	SysFatal(buffer->start == NULL, errno, "While trying to create input buffer");
	
	/* Initialize variables */
	buffer->available = size;
	buffer->write     = size;
	buffer->border    = buffer->start;
	buffer->current   = buffer->start;
	buffer->end       = buffer->start + size;
	
	/* Validate buffer integrity */
	Log2(debug, "Buffer created", "[input_buffer.c]{create}");
	assert(input_buffer_validate(buffer));

	return buffer;
}

void input_buffer_update (struct input_buffer *buffer, int read)
{
	buffer->current += read;
	buffer->available -= read;

	if (buffer->current == buffer->end)
	{
		buffer->current = buffer->start;
	}

	if (buffer->current < buffer->border)
	{
		buffer->write = buffer->border - buffer->current;
	}
	else
	{
		buffer->write = MIN(buffer->end - buffer->current, buffer->available);
	}

	/* Validate buffer integrity */
	Log2(debug, "Buffer state updated", "[input_buffer.c]{update}");
	assert(input_buffer_validate(buffer));
}

void input_buffer_print (struct input_buffer *buffer)
{
	if (buffer->current < buffer->border)
	{
		fprintf(stderr, "##DEBUG##{{{{%.*s||||%.*s}}}}\n", buffer->end - buffer->border, buffer->border, buffer->current - buffer->start, buffer->start);
	}
	else
	{
		fprintf(stderr, "##DEBUG##{{{{%.*s}}}}\n", buffer->current - buffer->border, buffer->border);
	}
}

int input_buffer_find (struct input_buffer *buffer, int c)
{
	int retval;
	char *index;

	Log2(debug, "Trying to find char in used buffer", "[input_buffer.c]{find}");
	input_buffer_print(buffer);

	retval = -1;
	index  = NULL;

	/* Validate buffer integrity */
	assert(input_buffer_validate(buffer));
	
	/* Check if message spans beyond the end */
	if (buffer->current <= buffer->border)
	{
		/* #### Message devided in 2 parts ####
		 *
		 * start ... current border ... end
		 * ----------------- --------------
		 *      head               tail
		 *
		 * tail + head = all read bytes in buffer
		 */

		/* Could be that the buffer is empty */
		if (buffer->available == buffer->end - buffer->start)
		{
			Log2(debug, "Buffer empty", "[input_buffer.c]{find}");
			return -1;
		}
		
		/* Search tail */
		index = memchr(buffer->border, c, buffer->end - buffer->border);

		/* Check if 'c' was present */
		if (index)
		{
			Log2(debug, "Found in split message (tail)", "[input_buffer.c]{find}");
			retval = index - buffer->border + 1;
fprintf(stderr, "DEBUG[input_buffer.c]{find}: index(%p) - border(%p) + 1 = retval(%d)\n", index, buffer->border, retval);
			return index - buffer->border + 1;
		}

		/* Search head */
		index = memchr(buffer->start, c, buffer->current - buffer->start);

		/* Check if 'c' was present */
		if (index)
		{
			Log2(debug, "Found in split message (head)", "[input_buffer.c]{find}");
			retval = (buffer->end - buffer->border) + (index - buffer->start) + 1;
fprintf(stderr, "DEBUG[input_buffer.c]{find}: ( end(%p) - border(%p) ) + ( index(%p) - start(%p) ) + 1 = retval(%d)\n", buffer->end, buffer->border, index, buffer->start, retval);
			return retval;
		}
	}
	else
	{
		/* #### Message in 1 part #### */
		index = memchr(buffer->border, c, buffer->current - buffer->border);

		if (index)
		{
			Log2(debug, "Found in single part message", "[input_buffer.c]{find}");
			retval = index - buffer->border + 1;
fprintf(stderr, "DEBUG[input_buffer.c]{find}: index(%p) - border(%p) + 1 = retval(%d)\n", index, buffer->border, retval);
			return retval;
		}
	}

	/* Apperently it was not found */
	Log2(debug, "Character not found in used buffer", "[input_buffer.c]{find}");
	retval = -1;
	return retval;
}

int input_buffer_validate (struct input_buffer *buffer)
{
	if (!(
		buffer != NULL
		&& buffer->start != NULL
		&& buffer->current != NULL
		&& buffer->border != NULL
		&& buffer->end != NULL
		&& buffer->current < buffer->end
		&& buffer->current >= buffer->start
		&& buffer->write <= buffer->available
		&& (
			(buffer->current < buffer->border && buffer->write == buffer->border - buffer->current && buffer->available == buffer->write)
			|| (buffer->current > buffer->border && buffer->write == buffer->end - buffer->current && buffer->available == (buffer->border - buffer->start) + buffer->write)
			|| (buffer->current == buffer->border && (buffer->available == 0 || buffer->available == buffer->end - buffer->start) && (buffer->write == 0 || buffer->write == buffer->end - buffer->current))
		   )
	   ) )
	{
		/* Buffer is corrupted */
		fprintf(stderr, "[input_buffer.c]{validate}: Buffer CORRUPT: start:%p border:%p end:%p current:%p size:%d front:%d back:%d diff:%d avail:%d write:%d\n", buffer->start, buffer->border, buffer->end, buffer->current, buffer->end - buffer->start, buffer->border - buffer->start, buffer->end - buffer->border, buffer->current - buffer->border, buffer->available, buffer->write);
		return FALSE;
	}
	
	/* Buffer is in good health */
	fprintf(stderr, "[input_buffer.c]{validate}: Buffer CORRECT: start:%p border:%p end:%p current:%p size:%d front:%d back:%d diff:%d avail:%d write:%d\n", buffer->start, buffer->border, buffer->end, buffer->current, buffer->end - buffer->start, buffer->border - buffer->start, buffer->end - buffer->border, buffer->current - buffer->border, buffer->available, buffer->write);
	Log2(debug, "Buffer is in good health", "[input_buffer.c]{validate}");
	return TRUE;
}

int input_buffer_purgeline (struct input_buffer *buffer)
{
	int len;

	Log2(debug, "Line purge requested", "[input_buffer.c]{purgeline}");
	len = input_buffer_find(buffer, '\n');

	/* Find a string that ends with a newline */
	if (len == -1)
	{
		/* Not found */
		
		/* Purge all */
		buffer->border    = buffer->start;
		buffer->current   = buffer->border;
		buffer->write     = buffer->end - buffer->current;
		buffer->available = buffer->write;
		
		/* We didn't delete it completely */
		Log2(debug, "Buffer purged, but line not", "[input_buffer.c]{purgeline}");
		/* Validate bufer integrity */
		assert(input_buffer_validate(buffer));
		return FALSE;
	}

	/* Do some black purging magic */
	assert(len > 0);
	buffer->border += len;
	if (buffer->border > buffer->end)
	{
		buffer->border = buffer->start - (buffer->border - buffer->end);
	}
	
	/* Calculate buffer->write */
	if (buffer->current < buffer->border)
	{
		buffer->write = buffer->border - buffer->current;
	}
	else
	{
		buffer->write = buffer->end - buffer->current;
	}
	
	/* Calculate buffer->available */
	buffer->available += len;
	
	/* We purged the whole line */
	Log2(debug, "Line purged succesfully", "[input_buffer.c]{purgeline}");
	/* Validate buffer integrity */
	assert(input_buffer_validate(buffer));
	return TRUE;
}

char *input_buffer_getline (struct input_buffer *buffer)
{
	char *line;
	int len;

	Log2(debug, "Request for line", "[input_buffer.c]{getline}");

	/* Find a string that ends with a newline */
	len = input_buffer_find(buffer, '\n');

	/* If it wasn't found */
	if (len == -1)
	{
		/* It was not found*/
		Log2(debug, "No line available", "[input_buffer.c]{getline}");
		return NULL;
	}

	/* Allocate memory for line */
	line = (char*) malloc (len + 1);
	SysFatal(line == NULL, errno, "While trying to allocate string for lineinput");
	
	if (buffer->border + len > buffer->end)
	{
		/* #### Message devided in 2 parts #### */
		int taillen = buffer->end - buffer->border;
		memcpy(line, buffer->border, taillen);
		memcpy(line + taillen, buffer->start, len - taillen);
		buffer->border = buffer->start + (len - taillen);
	}
	else
	{
		/* #### Message in 1 part #### */
		memcpy(line, buffer->border, len);
		buffer->border += len;

		/* In case it was exactly the end */
		if (buffer->border == buffer->end)
		{
			buffer->border = buffer->start;
		}	
	}
	
	/* Calculate buffer->write */
	if (buffer->current < buffer->border)
	{
		buffer->write = buffer->border - buffer->current;
	}
	else
	{
		buffer->write = buffer->end - buffer->current;
	}
	/* Calculate buffer->available */
	buffer->available += len;

	/* Make last character 0 */
	line[len] = '\0';

	/* Validate buffer integrity */
	Log2(debug, "Line read", "[input_buffer.c]{getline}");
	assert(input_buffer_validate(buffer));
	
	return line;
}

void input_buffer_free (struct input_buffer *buffer)
{
	/* Validate buffer integrity */
	Log2(debug, "Freeing the buffer", "[input_buffer.c]{free}");
	assert(buffer);
	assert(buffer->start);
	assert(input_buffer_validate(buffer));

	/* Free malloc()ed memory */
	free(buffer->start);
	buffer->start = NULL;
	
	free(buffer);
	buffer = NULL;
	
	Log2(debug, "Buffer freed", "[input_buffer.c]{free}");
}
