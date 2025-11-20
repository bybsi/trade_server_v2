#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include <stddef.h>
#include <pthread.h>

typedef struct ht_entry {
	char* key;
	void* value;
	struct ht_entry* next;	   // hash collision chain
} ht_entry_t;

typedef struct hashtable {
	// Num elements
	size_t size;
	// Max elements
	size_t capacity;
	ht_entry_t** buckets;
	// Entry cleanup function
	void (*free_func)(void*);
	pthread_mutex_t lock;
} hashtable_t;

hashtable_t* ht_init(size_t initial_capacity, void (*free_func)(void*));
void ht_destroy(hashtable_t* ht);
int ht_put(hashtable_t *ht, const char* key, void* value);
void* ht_get(hashtable_t *ht, const char* key);
void* ht_remove(hashtable_t* ht, const char* key);

#endif // _HASHTABLE_H__
