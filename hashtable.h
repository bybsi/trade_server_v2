#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include <stddef.h>
#include <pthread.h>

typedef struct ht_entry_t {
	char* key;
	void* value;
	struct ht_entry_t* next; // hash collision chain
	void* ref; /* reference to another containing datastructure node
		      that also holds a reference to this node.
		      In our case a DLL_NODE. */
} HT_ENTRY;

typedef struct hashtable_t {
	// Current number of elements
	size_t size;
	// Max number of elements
	size_t capacity;
	HT_ENTRY** buckets;
	// HT_ENTRY->value cleanup function
	void (*free_func)(void*);
	pthread_mutex_t lock;
} HASHTABLE;

HASHTABLE* ht_init(size_t initial_capacity, void (*free_func)(void*));
void ht_destroy(HASHTABLE *ht);
HT_ENTRY *ht_put(HASHTABLE *ht, const char* key, void* value);
void* ht_get(HASHTABLE *ht, const char* key);
void* ht_remove(HASHTABLE *ht, const char* key);

#endif // _HASHTABLE_H__
