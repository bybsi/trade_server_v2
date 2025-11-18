#ifndef _SSE_SERVER_H_
#define _SSE_SERVER_H_

#include <stddef.h>
#include <pthread.h>

#include "logger.h"
#include "sse_client_writer.h"

typedef struct sse_server {
	unsigned short port;
	unsigned short data_queue_size;
	char **data_queue;
	unsigned short data_insert_idx;
	ST_CLIENT_WRITER *client_writer;
} ST_SSE_SERVER;

ST_SSE_SERVER * sse_server_init(unsigned short port, unsigned short data_queue_size);
pthread_t sse_server_start(ST_SSE_SERVER *server);
void sse_server_queue_data(ST_SSE_SERVER * server, char *data);
void sse_server_stop(ST_SSE_SERVER *server);

#endif // _SSE_SERVER_H_
