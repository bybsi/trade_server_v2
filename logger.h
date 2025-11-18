#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <pthread.h>

typedef struct logger {
	FILE *fh;
	pthread_mutex_t lock;
} ST_LOGGER;

ST_LOGGER * logger_init(char *file_name);
void logger_write(ST_LOGGER *logger, char *message);
void logger_close(ST_LOGGER *logger);

#endif // _LOGGER_H_
