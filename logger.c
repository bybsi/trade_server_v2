#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "logger.h"

#define LOG_DIR "logs/"

ST_LOGGER * logger_init(char *file_name) {
	// TODO: rotating files
	FILE *fh;
	char filepath[128];
	ST_LOGGER *logger = malloc(sizeof(ST_LOGGER)); 

	if (pthread_mutex_init(&logger->lock, NULL) != 0) {
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

void logger_write(ST_LOGGER *logger, char *message) {
	pthread_mutex_lock(&logger->lock);
	if (logger->fh) { 
		time_t now;
		time(&now);
		fseek(logger->fh, 0, SEEK_END);
		fprintf(logger->fh, "[%s]\t%s\n", strtok(ctime(&now), "\n"), message);
		fflush(logger->fh);
	}
	pthread_mutex_unlock(&logger->lock);
}

void logger_close(ST_LOGGER *logger) {
	pthread_mutex_lock(&logger->lock);
	if (logger->fh) {
		fclose(logger->fh);
		logger->fh = NULL;
	}
	pthread_mutex_unlock(&logger->lock);
}

