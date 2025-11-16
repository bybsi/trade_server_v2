#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>

#include "sse_client_writer.h"

#define MAX_DATA_SEND_LEN 2048
#define NUM_CLIENT_LISTS 3
#define NUM_CLIENTS_PER_LIST 1000
/*
typedef struct client_list_t {
	int client_fd;
	struct client_list_t *next;
} ST_CLIENT_LIST;
*/

typedef struct client_list_manager_t {
	pthread_mutex_t lock;
	// +1 so we always end with a 0
	int client_fd_arr[NUM_CLIENTS_PER_LIST + 1];
	unsigned short last_insert_idx;
	unsigned short last_read_idx;
	unsigned short stop;
	char **data_queue;
	unsigned short data_queue_size;
	pthread_t thread_id;
} ST_CLIENT_LIST_MANAGER;

typedef struct client_writer_t {
	ST_CLIENT_LIST_MANAGER clm[NUM_CLIENT_LISTS];
	unsigned short round_robin_idx;
} ST_CLIENT_WRITER;



/*
ST_CLIENT_LIST_MANAGER * client_list_manager_init() {
	ST_CLIENT_LIST_MANAGER *clm = malloc(sizeof(ST_CLIENT_LIST_mANAGER));
	if (pthread_mutex_init(&clm->lock, NULL) != 0) {
		free(clm);
		return NULL;
	}
}
*/
/*
static ST_CLIENT_LIST * new_client_list_node(int client_fd) {
	ST_CLIENT_LIST *cl = malloc(sizeof(ST_CLIENT_LIST));
	cl->client_fd = client_fd;
	cl->next = NULL;
	return cl;
}
*/
ST_CLIENT_WRITER * client_writer_init(char **data_queue) {
	unsigned short i;
	ST_CLIENT_WRITER *client_writer = malloc(sizeof(ST_CLIENT_WRITER));
	client_writer->round_robin_idx = 0;
	for (i = 0; i < NUM_CLIENT_LISTS; i++) {
		if (pthread_mutex_init(&client_writer->clm[i].lock, NULL) != 0) {
			free(client_writer);
			return NULL;
		}
		memset(client_writer->clm[i].client_fd_arr, 0, 
			sizeof(client_writer->clm[i].client_fd_arr));
		client_writer->clm[i].last_insert_idx = 0;
		client_writer->clm[i].last_read_idx = 0;
		client_writer->clm[i].stop = 0;
		client_writer->clm[i].data_queue = data_queue;
	}
	return client_writer;
}

void start_client_writer(ST_CLIENT_WRITER *client_writer) {
	unsigned short i;
	for (i = 0; i < NUM_CLIENT_LISTS; i++) {
		pthread_create(&client_writer->clm[i].thread_id, NULL, client_write_thread, &client_writer->clm[i]);
	}
}

void stop_client_writer() {
	unsigned short i;
	int *status;
	for (i = 0; i < NUM_CLIENT_LISTS; i++) {		
		pthread_mutex_lock(&client_writer->clm[i].lock);
		client_writer->clm[i].stop = 1;
		pthread_mutex_unlock(&client_writer->clm[i].lock);
	}
	
	for (i = 0; i < NUM_CLIENT_LISTS; i++)
		pthread_join(client_writer->clm[i].thread_id, (void **) &status);
}

static void *client_write_thread(void *clm) {
	unsigned short i;
	int *clients;
	char **data;
	char send_buffer[MAX_DATA_SEND_LEN];

	ST_CLIENT_LIST_MANAGER *clm_ptr;
	
	clm_ptr = (ST_CLIENT_LIST_MANAGER *) clm;
	clients = clm_ptr->client_fd_arr;
	data    = clm_ptr->data_queue;
	while (1) {
		sleep(2);
		
		if (clm_ptr->stop) {
			// TODO Log
			return 0;
		}

		if (clm_ptr->last_read_idx == data_queue_size)
			clm_ptr->last_read_idx = 0;

		//pthread_mutex_lock(&clm->lock);
		strncpy(send_buffer, data[clm_ptr->last_read_idx], MAX_DATA_SEND_LEN);
		send_buffer[MAX_DATA_SEND_LEN - 1] = '\0';
		// 0  indicates the end of the list.
		// -1 indicates a lost connection.
		// TODO: linked list to drop bad connections.
		for (i = 0; clients[i] != 0; i++) {
			if (clients[i] == -1)
				continue;

			if (send_sse_event(clients[i], send_buffer) < 0) {
				// TODO Log error
				clients[i] = -1;
				close(clients[i]);
			}
		}
		//pthread_mutex_unlock(&clm->lock);

		clm_ptr->last_read_idx++;
	}
}

static ST_CLIENT_LIST_MANAGER * client_writer_get_manager(ST_CLIENT_WRITER *client_writer) {
	if (client_writer->round_robin_idx == NUM_CLIENT_LISTS)
		client_writer->round_robin_idx = 0;
	return &client_writer->clm[client_writer->round_robin_idx++];
}

static void client_writer_add_client(ST_CLIENT_WRITER *client_writer, int client_fd) {
	unsigned short i = 0;
	ST_CLIENT_LIST_MANAGER *clm = client_writer_get_manager(client_writer);

	/*
	// Sacrifice two slots for efficiency
	do {
		clm = client_writer_get_manager(client_writer);
	} while (
		clm->last_insert_idx == NUM_CLIENTS_PER_LIST && 
		client_writer->round_robin_idx != NUM_CLIENT_LISTS);
	*/

	if (clm->last_insert_idx == NUM_CLIENTS_PER_LIST)
		// log at capacity.
		return;

	//pthread_mutex_lock(&clm->lock);
	clm->client_fd_arr[clm->last_insert_idx++] = client_fd;
	//pthread_mutex_unlock(&clm->lock);
}

int send_sse_event(int client_fd, const char *data) {
	char buffer[BUFFER_SIZE];
	snprintf(buffer, sizeof(buffer), "data: %s\n\n", data);
	ssize_t bytes = write(client_fd, buffer, strlen(buffer));
	return (bytes == (ssize_t)strlen(buffer)) ? 0 : -1;
}
