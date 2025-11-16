#ifndef _SSE_CLIENT_WRITER_H_
#define _SSE_CLIENT_WRITER_H_

#include <stddef.h>
#include <pthread.h>

int send_sse_event(int client_fd, const char *data);
ST_CLIENT_WRITER client_writer_init();

#endif // _SSE_CLIENT_WRITER_H_
