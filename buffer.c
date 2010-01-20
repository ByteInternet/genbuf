#include "defines.h"
#include "buffer.h"

#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

#include "log.h"

static struct msgqueue *create_internal_buffer (struct msgqueue *prev)
{
	struct msgqueue *retval = (struct msgqueue*) malloc (sizeof(struct msgqueue));

	retval->next = NULL;

	if (prev)
		prev->next = retval;

	return retval;
}

static void buffer_push (struct buffer *buff, char *str)
{
	int oldstate;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
	pthread_mutex_lock(&(buff->mutex));

	buff->current->msgs[buff->endindex] = str;
	
	buff->endindex++;
	if (buff->endindex == REALLOC_SIZE)
	{
		buff->current = create_internal_buffer(buff->current);
		buff->endindex = 0;
	}

	pthread_mutex_unlock(&(buff->mutex));
	
	sem_post(&(buff->semaphore));
	pthread_setcancelstate(oldstate, NULL);

	pthread_testcancel();
}

static void buffer_unpop (struct buffer *buff, char *msg)
{
	int oldstate;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
	pthread_mutex_lock(&(buff->mutex));

	if (buff->headindex == 0)
	{
		struct msgqueue *oldhead = buff->head;
		
		buff->head = create_internal_buffer(NULL);
		buff->head->next = oldhead;
		
		buff->headindex = REALLOC_SIZE;
	}

	buff->headindex--;

	buff->head->msgs[buff->headindex] = msg;
	
	pthread_mutex_unlock(&(buff->mutex));
	
	sem_post(&(buff->semaphore));
	pthread_setcancelstate(oldstate, NULL);

	pthread_testcancel();
}

static char *buffer_pop (struct buffer *buff)
{
	int oldstate;
	char *retval;

	sem_wait(&(buff->semaphore));

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
	pthread_mutex_lock(&(buff->mutex));

	retval = buff->head->msgs[buff->headindex];
	buff->headindex++;

	if (buff->headindex == REALLOC_SIZE)
	{
		struct msgqueue *oldhead = buff->head;
		buff->head = buff->head->next;
		free(oldhead);
		buff->headindex = 0;
	}
	
	pthread_mutex_unlock(&(buff->mutex));
	pthread_setcancelstate(oldstate, NULL);

	pthread_testcancel();

	return retval;
}

static int buffer_size (struct buffer *buff)
{
	int retval;
	
	sem_getvalue(&(buff->semaphore), &retval);

	return retval;
}

struct buffer*
	buffer_init ()
{
	struct buffer *buff = (struct buffer*) malloc (sizeof(struct buffer));

	Fatal(buff == NULL, "Malloc returned NULL", "Buffer creation failed");
	
	sem_init(&(buff->semaphore), 0, 0);
	pthread_mutex_init(&(buff->mutex), NULL);

	buff->headindex = 0;
	buff->endindex  = 0;
	buff->head      = create_internal_buffer(NULL);
	buff->current   = buff->head;
	
	buff->push  = buffer_push;
	buff->pop   = buffer_pop;
	buff->unpop = buffer_unpop;
	buff->size  = buffer_size;

	return buff;
}

int buffer_cleanup(struct buffer *buff)
{
	int index, end;
	struct msgqueue *curr, *next;
	
	pthread_mutex_destroy(&(buff->mutex));
	sem_destroy(&(buff->semaphore));

	/* Cleanup buffers */
	index = buff->headindex;
	curr  = buff->head;
	end   = REALLOC_SIZE;
	while (curr)
	{
		next = curr->next;

		if (! next)
			end = buff->endindex;
		
		/* Cleanup remaining messages */
		while (index < end)
		{
			free(curr->msgs[index]);
			index++;
		}
		index = REALLOC_SIZE;
		
		free(curr);
		curr = next;
	}

	free(buff);

	return 0;
}
