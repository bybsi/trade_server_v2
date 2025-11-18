#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sse_client_writer.h"
/*
int send_sse_event(int client_fd, const char *data);
ST_CLIENT_WRITER client_writer_init(char **data_queue);
void client_writer_start(ST_CLIENT_WRITER *client_writer);
void client_writer_stop(ST_CLIENT_WRITER *client_writer);
void client_writer_add_client(ST_CLIENT_WRITER *client_writer, int client_fd);
*/

int main(int argc, char *argv[]) {
	int i;
	ST_CLIENT_WRITER *cw;
	char *data[] = {
		"data: HERE IS DATA STRING 1\n\n",
		"data: HERE IS DATA STRING 2\n\n",
		"data: HERE IS DATA STRING 3\n\n"
	};
	printf("%lu\n", sizeof(data) / sizeof(data[0]));
	cw = client_writer_init(data, sizeof(data) / sizeof(data[0]));

	for (i = 1; i < 5; i++)
		client_writer_add_client(cw, i);

	client_writer_start(cw);

	for (i = 5; i < 10; i++)
		client_writer_add_client(cw, i);

	sleep(7);
	client_writer_stop(cw);
	exit(0);
} 
