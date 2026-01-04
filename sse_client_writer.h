/**
	See SSEserver.png on github for more information
	bybsi/trade_server_v2/SSEserver.png
*/
#ifndef _SSE_CLIENT_WRITER_H_
#define _SSE_CLIENT_WRITER_H_

#include <stddef.h>
#include <pthread.h>

#include "logger.h"

#define MAX_DATA_SEND_LEN 2048
#define NUM_CLIENT_LISTS 3
#define NUM_CLIENTS_PER_LIST 1000

// TODO use a linked list instead
typedef struct client_data_node {
	unsigned long long id;
	char data[MAX_DATA_SEND_LEN];
} st_client_data_node_t;

typedef struct client_list_manager {
	pthread_mutex_t lock;
	// +1 so we always end with a 0
	int client_fd_arr[NUM_CLIENTS_PER_LIST + 1];
	unsigned short last_client_insert_idx;
	unsigned short last_data_read_idx;
	unsigned short stop;
	st_client_data_node_t *data_queue;
	unsigned short data_queue_size;
	//REMOVE
	unsigned short id;
	pthread_t thread_id;
} st_client_list_manager_t;

typedef struct client_writer {
	st_client_list_manager_t clm[NUM_CLIENT_LISTS];
	unsigned short round_robin_idx;
	st_client_data_node_t *data_queue;
	unsigned short data_queue_size;
	unsigned short last_data_write_idx;
	st_logger_t *logger;
} st_client_writer_t;

int send_sse_event(int client_fd, const char *data);
st_client_writer_t * client_writer_init(unsigned short data_queue_size);
void client_writer_start(st_client_writer_t *client_writer);
void client_writer_stop(st_client_writer_t *client_writer);
void client_writer_destroy(st_client_writer_t *client_writer);
void client_writer_add_client(st_client_writer_t *client_writer, int client_fd);
void client_writer_queue_data(st_client_writer_t *client_writer, char *data);

#endif // _SSE_CLIENT_WRITER_H_
