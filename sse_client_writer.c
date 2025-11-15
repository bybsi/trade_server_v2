#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>

#include "sse_client_writer.h"

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
	int client_fd_arr[NUM_CLIENTS_PER_LIST];
	unsigned short last_insert_idx;
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
ST_CLIENT_WRITER * client_writer_init() {
	unsigned short i;
	ST_CLIENT_WRITER *client_writer = malloc(sizeof(ST_CLIENT_WRITER));
	client_writer->round_robin_idx = 0;
	for (i = 0; i < NUM_CLIENT_LISTS; i++) {
		if (pthread_mutex_init(&client_writer->clm[i].lock, NULL) != 0) {
			free(client_writer);
			return NULL;
		}
		memset(client_writer->clm[i].client_fd_list, 0, 
			sizeof(client_writer->clm[i].client_fd_list));
		client_writer->clm[i].last_insert_idx = 0;
	}
	return client_writer;
}

static ST_CLIENT_LIST_MANAGER * client_writer_get_manager(ST_CLIENT_WRITER *client_writer) {
	if (client_writer->round_robin_idx == NUM_CLIENT_LISTS)
		client_writer->round_robin_idx = 0;
	return &client_writer->clm[client_writer->round_robin_idx++];
}

static void client_writer_add(ST_CLIENT_WRITER *client_writer, int client_fd) {
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

	pthread_mutex_lock(clm->lock);
	//TODO
}

int send_sse_event(int client_fd, const char *data) {
	char buffer[BUFFER_SIZE];
	snprintf(buffer, sizeof(buffer), "data: %s\n\n", data);
	ssize_t bytes = write(client_fd, buffer, strlen(buffer));
	return (bytes == (ssize_t)strlen(buffer)) ? 0 : -1;
}
