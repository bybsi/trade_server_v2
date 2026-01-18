#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include <unistd.h>

#include "sse_server.h"

#define PORT 6262
#define DATA_Q_SIZE 10

int main(int argc, char *argv[]) {
	int i;
	pthread_t server_tid;
	st_client_writer_t *cw;
	st_sse_server_t *server;
	
	char buffer[1024];
	char *data[] = {
		"data 1",
		"data 2",
		"data data 3",
		NULL
	};

	server = sse_server_init(PORT, DATA_Q_SIZE);
	if (!server) {
		printf("Could not initialize server.\n");
		exit(0);
	}
	server_tid = sse_server_start(server);
	
	printf("Test server started...\n");
	
	for (i = 0; data[i]; i++) {
		sleep(1);
		snprintf(buffer, 1024, "event: Y\ndata: %s\n\n", data[i]);
		sse_server_queue_data(server, buffer);
	}
	
	for (i = 0; data[i]; i++) {
		sleep(1);
		snprintf(buffer, 1024, "event: Y\ndata: %s\n\n", data[i]);
		sse_server_queue_data(server, buffer);
	}

	sleep(20);
	sse_server_stop(server);
	exit(0);
} 
