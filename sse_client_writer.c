#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <time.h>
#include <pthread.h>

#include "sse_client_writer.h"
#include "logger.h"

/*
typedef struct client_list {
	int client_fd;
	struct client_list *next;
} ST_CLIENT_LIST;
*/

static int verbose = 1;

static ST_CLIENT_LIST_MANAGER * client_writer_select_manager(ST_CLIENT_WRITER *client_writer) {
	if (client_writer->round_robin_idx == NUM_CLIENT_LISTS)
		client_writer->round_robin_idx = 0;
	return &client_writer->clm[client_writer->round_robin_idx++];
}

void *client_write_thread(void *clm) {
	unsigned short i;
	unsigned long long last_data_id;
	int *clients;
	char send_buffer[MAX_DATA_SEND_LEN], log_file_name[64];

	ST_LOGGER              *logger;
	ST_CLIENT_LIST_MANAGER *clm_ptr;
	ST_CLIENT_DATA_NODE    *data;
	
	clm_ptr = (ST_CLIENT_LIST_MANAGER *) clm;
	
	snprintf(log_file_name, 64, "write_thread_%d.log", clm_ptr->id);
	logger = logger_init(log_file_name);

	clients = clm_ptr->client_fd_arr;
	data    = clm_ptr->data_queue;
	last_data_id = 0;
	while (1) {
		if (verbose)
			printf("In client_write_thread(%d)\n", clm_ptr->id);

		sleep(2);
		
		if (clm_ptr->stop) {
			logger_write(logger, "Got stop flag.");
			logger_close(logger);
			return 0;
		}

		if (clm_ptr->last_read_idx == clm_ptr->data_queue_size)
			clm_ptr->last_read_idx = 0;

		//pthread_mutex_lock(&clm->lock);

		// Send the most recent record if it hasn't been sent.
		// This also handles cases where thre is no data
		// at the current index because the timestamp(id) is still 0.
		if (last_data_id >= data[clm_ptr->last_read_idx].id)
			continue;
		last_data_id = data[clm_ptr->last_read_idx].id;

		memcpy(send_buffer, data[clm_ptr->last_read_idx].data, MAX_DATA_SEND_LEN);
		send_buffer[MAX_DATA_SEND_LEN - 1] = '\0';

		if (verbose)
			printf("Sending(%d), %s\n", clm_ptr->id, send_buffer);

		// 0  indicates the end of the list.
		// -1 indicates a lost connection.
		// TODO: linked list to drop bad connections.
		for (i = 0; clients[i] != 0; i++) {
			if (clients[i] == -1)
				continue;

			if (send_sse_event(clients[i], send_buffer) < 0) {
				// TODO variable length arguments to logger_write like printf()
				logger_write(logger, "Could not send SSE event:");
				logger_write(logger, send_buffer);
				// close(clients[i]); // causes crash...
				clients[i] = -1;
			}
		}
		//pthread_mutex_unlock(&clm->lock);

		clm_ptr->last_read_idx++;
	}
}

ST_CLIENT_WRITER * client_writer_init(unsigned short data_queue_size) {
	unsigned short i;
	char log_file_name[64];
	ST_CLIENT_WRITER *client_writer = malloc(sizeof(ST_CLIENT_WRITER));
	client_writer->round_robin_idx = 0;
	client_writer->last_write_idx = 0;
	client_writer->data_queue_size = data_queue_size;
	client_writer->data_queue = malloc(data_queue_size * sizeof(ST_CLIENT_DATA_NODE));
	for (i = 0; i < data_queue_size; i++) {
		client_writer->data_queue[i].id = 0;
		client_writer->data_queue[i].data[0] = '\0';
	}

	for (i = 0; i < NUM_CLIENT_LISTS; i++) {
		if (pthread_mutex_init(&client_writer->clm[i].lock, NULL) != 0) {
			free(client_writer->data_queue);
			free(client_writer);
			return NULL;
		}
		memset(client_writer->clm[i].client_fd_arr, 0, 
			sizeof(client_writer->clm[i].client_fd_arr));
		client_writer->clm[i].last_insert_idx = 0;
		client_writer->clm[i].last_read_idx = 0;
		client_writer->clm[i].stop = 0;

		client_writer->clm[i].data_queue = client_writer->data_queue;
		client_writer->clm[i].data_queue_size = data_queue_size;
	
		//REMOVE
		client_writer->clm[i].id = i+1;
	}
	
	snprintf(log_file_name, 64, "client_writer.log");
	client_writer->logger = logger_init(log_file_name);
	logger_write(client_writer->logger, "Started client writer.");
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
	
	logger_write(client_writer->logger, "Stopped client writer.");
	logger_close(client_writer->logger);
}

void client_writer_add_client(ST_CLIENT_WRITER *client_writer, int client_fd) {
	unsigned short i = 0;
	static unsigned short capacity_logged = 0;
	ST_CLIENT_LIST_MANAGER *clm = client_writer_select_manager(client_writer);

	/*
	// Sacrifice two slots for efficiency
	do {
		clm = client_writer_select_manager(client_writer);
	} while (
		clm->last_insert_idx == NUM_CLIENTS_PER_LIST && 
		client_writer->round_robin_idx != NUM_CLIENT_LISTS);
	*/

	if (clm->last_insert_idx == NUM_CLIENTS_PER_LIST) {
		// log at capacity.
		if (!capacity_logged) {
			char buffer[128];
			snprintf(buffer, 128, "Capacity reached for writer(%d)", clm->id);
			logger_write(client_writer->logger, buffer);
			capacity_logged = 1;
		}
		return;
	}

	//pthread_mutex_lock(&clm->lock);
	if (verbose)
		printf("\tAdding client(%d) to writer(%d)\n", client_fd, clm->id);

	clm->client_fd_arr[clm->last_insert_idx++] = client_fd;
	//pthread_mutex_unlock(&clm->lock);
}
	
void client_writer_queue_data(ST_CLIENT_WRITER *client_writer, char *data) {
	ST_CLIENT_DATA_NODE *data_node;
	if (client_writer->last_write_idx == client_writer->data_queue_size)
		client_writer->last_write_idx = 0;
	
	if (verbose)
		printf("adding data to %d, %s", client_writer->last_write_idx, data);

	data_node = &client_writer->data_queue[client_writer->last_write_idx++];
	data_node->id = time(NULL);
	memcpy(data_node->data, data, MAX_DATA_SEND_LEN);
	data_node->data[MAX_DATA_SEND_LEN - 1] = '\0';
}

void client_writer_destroy(ST_CLIENT_WRITER *client_writer) {
	free(client_writer->data_queue);
	free(client_writer);
}

int send_sse_event(int client_fd, const char *data) {
/*
	input data needs to be pre formatted to avoid this
	char buffer[2048];
	snprintf(buffer, sizeof(buffer), "data: %s\n\n", data);
	ssize_t bytes = write(client_fd, buffer, strlen(buffer));
*/
	return -1;
	ssize_t data_len = strlen(data);
	ssize_t bytes = write(client_fd, data, data_len);
	return (bytes == data_len) ? 0 : -1;
}
