#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include <unistd.h>

#include "hashtable.h"

#define PORT 6262
#define DATA_Q_SIZE 10

static void htfree(void *data) {
	free(data);
}

int main(int argc, char *argv[]) {
	int i;
	hashtable_t *ht;

	//ht = ht_init(10000, NULL);
	ht = ht_init(10000, NULL);

	ht_put(ht, "doubleu", strdup("bob"));
	printf("Got: %s\n", (char *)ht_get(ht, "doubleu"));
	printf("Got: %s\n", (char *)ht_get(ht, "woubleu"));
	ht_put(ht, "111", strdup("aaa"));
	printf("Got: %s\n", (char *)ht_get(ht, "111"));
	printf("Got: %s\n", (char *)ht_get(ht, "aaaa"));
	printf("Got: %s\n", (char *)ht_get(ht, "doubleu"));
	printf("Removing 111\n");
	ht_remove(ht, "111");
	printf("Got: %s\n", (char *)ht_get(ht, "111"));
	printf("Got: %s\n", (char *)ht_get(ht, "aaaa"));
	printf("Got: %s\n", (char *)ht_get(ht, "doubleu"));
	ht_destroy(ht);
	exit(0);
} 
