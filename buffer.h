#ifndef GENCACHE_BUFFER_H
#define GENCACHE_BUFFER_H

#include <pthread.h>
#include <semaphore.h>

#include "types.h"

/* Internal singly linked msgqueue */
#define REALLOC_SIZE 1000
struct msgqueue {
	char *msgs[REALLOC_SIZE];
	struct msgqueue *next;
};

/* Our buffer definition */
struct buffer {
	sem_t semaphore;
	pthread_mutex_t mutex;

	int headindex;
	int endindex;
	struct msgqueue *head;
	struct msgqueue *current;

	void  (*push)  (struct buffer*, char *str);
	char *(*pop)   (struct buffer*);
	void  (*unpop) (struct buffer*, char *str);
	int   (*size)  (struct buffer*);
};

extern struct buffer*
	buffer_init ();
extern int
	buffer_cleanup (struct buffer*);

#endif /* GENCACHE_BUFFER_H */
