#ifndef _SSE_SERVER_H_
#define _SSE_SERVER_H_

#include <stddef.h>
#include <pthread.h>

#include "logger.h"
#include "sse_client_writer.h"


typedef struct sse_server {
	unsigned short port;
	st_logger_t *logger;
	st_client_writer_t *client_writer;
} st_sse_server_t;

st_sse_server_t * sse_server_init(unsigned short port, unsigned short data_queue_size);
pthread_t sse_server_start(st_sse_server_t *server);
void sse_server_queue_data(st_sse_server_t * server, char *data);
void sse_server_stop(st_sse_server_t *server);

#endif // _SSE_SERVER_H_
