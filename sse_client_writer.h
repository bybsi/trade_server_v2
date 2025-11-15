#ifndef _SSE_CLIENT_WRITER_H_
#define _SSE_CLIENT_WRITER_H_

#include <stddef.h>
#include <pthread.h>

// Set up a write thread for each client list
#define NUM_CLIENT_LISTS 3

typedef struct client_list {
	int client_fd;
	struct client_list *next;
} ST_CLIENT_LIST;

typedef struct client_list_manager {
	pthread_mutex_t lock;
	ST_CLIENT_LIST *head;
	ST_CLIENT_LIST *tail;
	unsigned int capacity;
} ST_CLIENT_LIST_MANAGER;

typedef struct client_writer {
	ST_CLIENT_LIST_MANAGER clm[NUM_CLIENT_LISTS];
	unsigned short round_robin_idx;
} ST_CLIENT_WRITER;

int send_sse_event(int client_fd, const char *data);
ST_CLIENT_WRITER client_writer_init();

#endif // _SSE_CLIENT_WRITER_H_
