#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <pthread.h>

#include "sse_client_writer.h"

/*
typedef struct client_list_t {
	int client_fd;
	struct client_list_t *next;
} ST_CLIENT_LIST;
*/

static ST_CLIENT_LIST_MANAGER * client_writer_get_manager(ST_CLIENT_WRITER *client_writer) {
	if (client_writer->round_robin_idx == NUM_CLIENT_LISTS)
		client_writer->round_robin_idx = 0;
	return &client_writer->clm[client_writer->round_robin_idx++];
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
		printf("In client_write_thread(%d)\n", clm_ptr->id);
		sleep(2);
		
		if (clm_ptr->stop) {
			// TODO Log
			printf("Got stop flag(%d)\n", clm_ptr->id);
			return 0;
		}

		if (clm_ptr->last_read_idx == clm_ptr->data_queue_size)
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

ST_CLIENT_WRITER * client_writer_init(char **data_queue, unsigned short data_queue_size) {
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
		client_writer->clm[i].data_queue_size = data_queue_size;
		printf("dq size: %d\n", client_writer->clm[i].data_queue_size);

		//REMOVE
		client_writer->clm[i].id = i+1;
	}
	return client_writer;
}

void client_writer_start(ST_CLIENT_WRITER *client_writer) {
	unsigned short i;
	for (i = 0; i < NUM_CLIENT_LISTS; i++) {
		pthread_create(&client_writer->clm[i].thread_id, NULL, client_write_thread, &client_writer->clm[i]);
	}
}

void client_writer_stop(ST_CLIENT_WRITER *client_writer) {
	unsigned short i;
	int *status;
	for (i = 0; i < NUM_CLIENT_LISTS; i++) {		
		pthread_mutex_lock(&client_writer->clm[i].lock);
		client_writer->clm[i].stop = 1;
		pthread_mutex_unlock(&client_writer->clm[i].lock);
	}
	
	for (i = 0; i < NUM_CLIENT_LISTS; i++)
		pthread_join(client_writer->clm[i].thread_id, (void **) &status);

	printf("client writer stopped\n");
}

void client_writer_add_client(ST_CLIENT_WRITER *client_writer, int client_fd) {
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

	if (clm->last_insert_idx == NUM_CLIENTS_PER_LIST) {
		// log at capacity.
		printf("\t\tCapacity reached for writer(%d)\n", clm->id);
		return;
	}

	//pthread_mutex_lock(&clm->lock);
	printf("\tAdding client(%d) to writer(%d)\n", clm->id, client_fd);
	clm->client_fd_arr[clm->last_insert_idx++] = client_fd;
	//pthread_mutex_unlock(&clm->lock);
}

int send_sse_event(int client_fd, const char *data) {
/*
	input data needs to be pre formatted to avoid this
	char buffer[2048];
	snprintf(buffer, sizeof(buffer), "data: %s\n\n", data);
	ssize_t bytes = write(client_fd, buffer, strlen(buffer));
*/
	ssize_t data_len = strlen(data);
	ssize_t bytes = write(client_fd, data, data_len);
	return (bytes == data_len) ? 0 : -1;
}
