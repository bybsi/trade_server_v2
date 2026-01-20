/**
	See SSEserver.png on github for more information
	bybsi/trade_server_v2/SSEserver.png
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <time.h>
#include <pthread.h>

#include "sse_client_writer.h"
#include "logger.h"

/* neat */
static int verbose = 1;

/* data queue lock */
pthread_mutex_t dq_lock;

/*
Chooses an available client write thread.

Params
	client_writer: The st_client_writer_t instance

Returns
	A pointer to the client list manager being used by the thread.
*/
static st_client_list_manager_t * client_writer_select_manager(st_client_writer_t *client_writer) {
	if (client_writer->round_robin_idx == NUM_CLIENT_LISTS)
		client_writer->round_robin_idx = 0;
	return &client_writer->clm[client_writer->round_robin_idx++];
}

/*
The thread worker function used to write data to clients.

It reads the most recent data from the shared data queue and
sends it to all clients managed by the associated st_client_list_manager_t.

Params
	clm: pointer that needs to cast to an st_client_list_manager_t

*/
void *client_write_thread(void *clm) {
	unsigned short i;
	unsigned long long last_data_id;
	int *clients;
	char send_buffer[MAX_DATA_SEND_LEN], log_file_name[64];

	st_logger_t              *logger;
	st_client_list_manager_t *clm_ptr;
	st_client_data_node_t    *data;
	
	clm_ptr = (st_client_list_manager_t *) clm;
	
	snprintf(log_file_name, 64, "write_thread_%d.log", clm_ptr->id);
	logger = logger_init(log_file_name);

	clients = clm_ptr->client_fd_arr;
	data    = clm_ptr->data_queue;
	last_data_id = 0;
	while (1) {
		if (verbose)
			printf("In client_write_thread(%d)\n", clm_ptr->id);

		sleep(2);
		
		pthread_mutex_lock(&clm_ptr->lock);
		if (clm_ptr->stop) {
			pthread_mutex_unlock(&clm_ptr->lock);
			logger_write(logger, "Got stop flag.");
			logger_close(logger);
			return 0;
		}
		pthread_mutex_unlock(&clm_ptr->lock);

		while (1) {
			if (clm_ptr->last_data_read_idx == clm_ptr->data_queue_size)
				clm_ptr->last_data_read_idx = 0;

			// Send new messages to the clients.
			// This also handles cases where there is no data
			// at the current index because the data 
			// id is 0 in that case.
			pthread_mutex_lock(&dq_lock);
			if (last_data_id >= data[clm_ptr->last_data_read_idx].id) {
				pthread_mutex_unlock(&dq_lock);
				break;
			}
			last_data_id = data[clm_ptr->last_data_read_idx].id;
			memcpy(send_buffer, data[clm_ptr->last_data_read_idx].data, MAX_DATA_SEND_LEN);
			pthread_mutex_unlock(&dq_lock);

			send_buffer[MAX_DATA_SEND_LEN - 1] = '\0';
			if (verbose)
				printf("Sending(%d), %s", clm_ptr->id, send_buffer);

			// 0  indicates the end of the list.
			// -1 indicates a lost connection.
			// TODO: linked list to drop bad connections.
			for (i = 0; clients[i] != 0; i++) {
				if (clients[i] == -1)
					continue;

				if (send_sse_event(clients[i], send_buffer) < 0) {
					logger_write(logger, "Could not send SSE event to client %d: %s", clients[i], send_buffer);
					// close(clients[i]); // causes crash...
					clients[i] = -1;
				}
			}

			clm_ptr->last_data_read_idx++;
		}
	}
}

/*
Initializes a new client writer.

Params
	data_queue_size: The max capacity of the shared data queue.

Returns
	A pointer to the st_client_writer_t instance.
	NULL on error.
*/
st_client_writer_t * client_writer_init(unsigned short data_queue_size) {
	unsigned short i;
	char log_file_name[64];
	st_client_writer_t *client_writer = malloc(sizeof(st_client_writer_t));
	client_writer->round_robin_idx = 0;
	client_writer->last_data_write_idx = 0;
	client_writer->data_queue_size = data_queue_size;
	client_writer->msg_count = 0;

	if (pthread_mutex_init(&dq_lock, NULL) != 0) {
		free(client_writer);
		return NULL;
	}

	client_writer->data_queue = malloc(data_queue_size * sizeof(st_client_data_node_t));
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
		client_writer->clm[i].last_client_insert_idx = 0;
		client_writer->clm[i].last_data_read_idx = 0;
		client_writer->clm[i].stop = 0;

		client_writer->clm[i].data_queue = client_writer->data_queue;
		client_writer->clm[i].data_queue_size = data_queue_size;
	
		client_writer->clm[i].id = i+1;
	}
	
	snprintf(log_file_name, 64, "client_writer.log");
	client_writer->logger = logger_init(log_file_name);
	logger_write(client_writer->logger, "Started client writer.");
	return client_writer;
}

/*
Starts a worker thread to handle each client list.

Params
	client_writer: The st_client_writer_t instance.
*/
void client_writer_start(st_client_writer_t *client_writer) {
	unsigned short i;
	for (i = 0; i < NUM_CLIENT_LISTS; i++) {
		pthread_create(
			&client_writer->clm[i].thread_id, NULL, 
			client_write_thread, &client_writer->clm[i]);
	}
}

/*
Stops all client writers and waits for them to rejoin the main thread.

Params
	client_writer: The st_client_writer_t instance.
*/
void client_writer_stop(st_client_writer_t *client_writer) {
	unsigned short i;
	for (i = 0; i < NUM_CLIENT_LISTS; i++) {
		pthread_mutex_lock(&client_writer->clm[i].lock);
		client_writer->clm[i].stop = 1;
		pthread_mutex_unlock(&client_writer->clm[i].lock);
	}
	logger_write(client_writer->logger, "Joining client writers\n");
	for (i = 0; i < NUM_CLIENT_LISTS; i++)
		pthread_join(client_writer->clm[i].thread_id, NULL);
	
	logger_write(client_writer->logger, "Stopped client writer.");
	logger_close(client_writer->logger);
}

/*
Adds a newly conneted client to the next available client manager.

Params
	client_writer: the st_client_writer_t instance.
	client_fd: file descriptor of the newly connected client.
*/
void client_writer_add_client(st_client_writer_t *client_writer, int client_fd) {
	unsigned short i = 0;
	static unsigned short capacity_logged = 0;
	st_client_list_manager_t *clm = client_writer_select_manager(client_writer);

	/*
	// Sacrifice two slots for efficiency
	do {
		clm = client_writer_select_manager(client_writer);
	} while (
		clm->last_client_insert_idx == NUM_CLIENTS_PER_LIST && 
		client_writer->round_robin_idx != NUM_CLIENT_LISTS);
	*/

	if (clm->last_client_insert_idx == NUM_CLIENTS_PER_LIST) {
		// Tells us that thread capacity has been reached.
		// Only log this one time.
		if (!capacity_logged) {
			logger_write(client_writer->logger, 
				"Capacity reached for writer(%d)",
				clm->id);
			capacity_logged = 1;
		}
		return;
	}

	if (verbose)
		printf("\tAdding client(%d) to writer(%d)\n", client_fd, clm->id);
	clm->client_fd_arr[clm->last_client_insert_idx++] = client_fd;
}

/*
Appends data to the shared data queue, which is read by the client write
threads to send data to clients.

Params
	client_writer: The st_client_writer_t instance.
	data: The data to append to the queue.
*/
void client_writer_queue_data(st_client_writer_t *client_writer, char *data) {
	st_client_data_node_t *data_node;

	pthread_mutex_lock(&dq_lock);
	if (client_writer->last_data_write_idx == client_writer->data_queue_size)
		client_writer->last_data_write_idx = 0;
	if (verbose)
		printf("adding data to %d, %s", client_writer->last_data_write_idx, data);
	data_node = &client_writer->data_queue[client_writer->last_data_write_idx++];
	// This will allow us to see batched messages in the log files. e.g.
	// the first 10 digits of the ID will match.
	data_node->id = time(NULL) + client_writer->msg_count;
	memcpy(data_node->data, data, MAX_DATA_SEND_LEN);
	data_node->data[MAX_DATA_SEND_LEN - 1] = '\0';
	
	pthread_mutex_unlock(&dq_lock);
}

void client_writer_destroy(st_client_writer_t *client_writer) {
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
	ssize_t data_len = strlen(data);
	ssize_t bytes = write(client_fd, data, data_len);
	return (bytes == data_len) ? 0 : -1;
}

