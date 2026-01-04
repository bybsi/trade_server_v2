#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include "logger.h"

#define LOG_DIR "logs/"

/*
Initializes a log file. Files are written to the directory
defined by #define LOG_DIR.

Params
	file_name: the filename, not containing a path.

Returns
	A pointer to and st_logger_t struct.
	NULL if there is an error.
*/
st_logger_t * logger_init(char *file_name) {
	// TODO: rotating files
	FILE *fh;
	char filepath[128];
	st_logger_t *logger = malloc(sizeof(st_logger_t)); 

	if (pthread_mutex_init(&logger->lock, NULL) != 0) {
		fprintf(stderr, "Could not init logger mutex\n");
		free(logger);
		return NULL;
	}

	snprintf(filepath, 128, LOG_DIR "%s", file_name);
	//if (!(fh = fopen(filepath, "a"))) {
	if (!(fh = fopen(filepath, "w"))) {
		fprintf(stderr, "Could not initialize logger: %s\n", filepath);
		free(logger);
		return NULL;
	}

	logger->fh = fh;
	return logger;
}

/*
Writes to the log file that's attached to the given st_logger_t instance.
The printf() syntax and formatting can be used. e.g.
logger_write(logger, "%s%d", chr_ptr, number);

Params
	logger: The st_logger_t instance.
	message: The log message format string.
	args: Optional variables to be applied to the format string.
*/
void logger_write(st_logger_t *logger, char *message, ...) {
	pthread_mutex_lock(&logger->lock);
	if (logger->fh) { 
		va_list args;
		time_t now;
		time(&now);
		fseek(logger->fh, 0, SEEK_END);
		// Timestamp
		fprintf(logger->fh, "[%s]\t", strtok(ctime(&now), "\n"));
		// This allows logger_write to be called like printf()
		// logger_write(logger, "%s%s", str1, str2);
		va_start(args, message);
		vfprintf(logger->fh, message, args);
		va_end(args);

		fprintf(logger->fh, "\n");
		fflush(logger->fh);
	}
	pthread_mutex_unlock(&logger->lock);
}

/*
Closes the log file.

Params
	logger: The st_logger_t instance.
*/
void logger_close(st_logger_t *logger) {
	pthread_mutex_lock(&logger->lock);
	if (logger->fh) {
		fclose(logger->fh);
		logger->fh = NULL;
	}
	pthread_mutex_unlock(&logger->lock);
}

