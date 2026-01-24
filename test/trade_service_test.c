#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "sse_server.h"
#include "trade_service.h"

#define PORT 6262
#define DATA_Q_SIZE 100

int main() {
	st_trade_service_t *service;
	st_sse_server_t *server;
	pthread_t server_tid;
	
	server = sse_server_init(PORT, DATA_Q_SIZE);
	if (!server) {
		fprintf(stderr, "Could not initialize server.\n");
		exit(255);
	}
	server_tid = sse_server_start(server);

	service = trade_service_init(server);

	if (!trade_service_start()) {
		fprintf(stderr, "no start\n");
	}

	sleep(120);
	trade_service_stop();
	trade_service_destroy();

	return 0;
}

