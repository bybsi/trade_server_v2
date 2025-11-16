#ifndef _SSE_CLIENT_WRITER_H_
#define _SSE_CLIENT_WRITER_H_

#include <stddef.h>
#include <pthread.h>

#define MAX_DATA_SEND_LEN 2048
#define NUM_CLIENT_LISTS 3
#define NUM_CLIENTS_PER_LIST 1000

typedef struct client_list_manager_t {
	pthread_mutex_t lock;
	// +1 so we always end with a 0
	int client_fd_arr[NUM_CLIENTS_PER_LIST + 1];
	unsigned short last_insert_idx;
	unsigned short last_read_idx;
	unsigned short stop;
	char **data_queue;
	unsigned short data_queue_size;
	//REMOVE
	unsigned short id;
	pthread_t thread_id;
} ST_CLIENT_LIST_MANAGER;

typedef struct client_writer_t {
	ST_CLIENT_LIST_MANAGER clm[NUM_CLIENT_LISTS];
	unsigned short round_robin_idx;
} ST_CLIENT_WRITER;

int send_sse_event(int client_fd, const char *data);
ST_CLIENT_WRITER * client_writer_init(char **data_queue, unsigned short data_queue_size);
void client_writer_start(ST_CLIENT_WRITER *client_writer);
void client_writer_stop(ST_CLIENT_WRITER *client_writer);
void client_writer_add_client(ST_CLIENT_WRITER *client_writer, int client_fd);

#endif // _SSE_CLIENT_WRITER_H_
