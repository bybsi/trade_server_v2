#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "trade_service.h"
#include "sse_server.h"

#define PORT 6262
#define DATA_Q_SIZE 10

/* TODO
void handle_signal(int sig) {
	if (service) {
		trade_service_stop(service);
	}
	exit(0);
}
*/

int main(void) {
	ST_SSE_SERVER *server;
	ST_TRADE_SERVICE *service;
	// TODO
	//signal(SIGINT, handle_signal);
	//signal(SIGTERM, handle_signal);
	
	server = sse_server_init(PORT, DATA_Q_SIZE);
	if (!server) {
		fprintf(stderr, "Could not initialize server.\n");
		exit(255);
	}
	server_tid = sse_server_start(server);
	
	service = trade_service_init(server);
	if (!trade_service_start(service)) {
		fprintf(stderr, "Could not initialize trade service.\n");
		// TODO signal stop SSE SERVER.
		//sse_server_stop(server);
		exit(255);
	}
	
	printf("SSE server started. Listening on port " PORT "\n");
	printf("Trade service started.\n");

	while (1) {
		// TODO
		sleep(5);
	}

	return 0;
}





