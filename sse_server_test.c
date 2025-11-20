#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include <unistd.h>

#include "sse_server.h"
/*
int send_sse_event(int client_fd, const char *data);
ST_CLIENT_WRITER client_writer_init(char **data_queue);
void client_writer_start(ST_CLIENT_WRITER *client_writer);
void client_writer_stop(ST_CLIENT_WRITER *client_writer);
void client_writer_add_client(ST_CLIENT_WRITER *client_writer, int client_fd);
*/

#define PORT 6262
#define DATA_Q_SIZE 10

int main(int argc, char *argv[]) {
	int i;
	pthread_t server_tid;
	ST_CLIENT_WRITER *cw;
	ST_SSE_SERVER *server;
	
	server = sse_server_init(PORT, DATA_Q_SIZE);
	if (!server) {
		printf("Could not initialize server.\n");
		exit(0);
	}
	server_tid = sse_server_start(server);
	
	printf("Test server started...\n");
	
	for (i = 1; i < 5; i++)
		client_writer_add_client(server->client_writer, i);
	sleep(2);
	printf("Slept for two seconds... adding data\n");
	sse_server_queue_data(server, "data 1\n");
	sse_server_queue_data(server, "data 2\n");
	sse_server_queue_data(server, "data 3\n");

	sleep(5);
	sse_server_stop(server);
	exit(0);
} 
