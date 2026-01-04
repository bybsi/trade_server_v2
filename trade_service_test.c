#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "trade_service.h"

int main() {
	st_trade_service_t *service;

	service = trade_service_init(NULL);

	if (!trade_service_start(service)) {
		fprintf(stderr, "no start\n");
	}

	sleep(10);
	trade_service_stop(service);
	trade_service_destroy(service);

	return 0;
}

