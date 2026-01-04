#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <pthread.h>

typedef struct logger {
	FILE *fh;
	pthread_mutex_t lock;
} st_logger_t;

st_logger_t * logger_init(char *file_name);
void logger_write(st_logger_t *logger, char *message, ...);
void logger_close(st_logger_t *logger);

#endif // _LOGGER_H_
