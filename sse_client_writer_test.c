#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sse_client_writer.h"

int main(int argc, char *argv[]) {
	int i;
	ST_CLIENT_WRITER *cw;
	char *data[] = {
		"data: HERE IS DATA STRING 1\n\n",
		"data: HERE IS DATA STRING 2\n\n",
		"data: HERE IS DATA STRING 3\n\n",
		NULL
	};
	//printf("%lu\n", sizeof(data) / sizeof(data[0]));
	cw = client_writer_init(10);

	for (i = 100; i < 105; i++)
		client_writer_add_client(cw, i);

	client_writer_start(cw);

	for (i = 105; i < 110; i++)
		client_writer_add_client(cw, i);

	for (i = 0; data[i]; i++) {
		client_writer_queue_data(cw, data[i]);
		sleep(1);
	}

	sleep(7);
	client_writer_stop(cw);
	client_writer_destroy(cw);
	exit(0);
} 
