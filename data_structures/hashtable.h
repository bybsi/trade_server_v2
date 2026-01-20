#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include <stddef.h>
#include <pthread.h>

typedef struct ht_entry {
	char* key;
	void* value;
	struct ht_entry* next; // hash collision chain
	void* ref; /* reference to another containing datastructure node
		      that also holds a reference to this node.
		      In our case a dll_node_t. */
} ht_entry_t;

typedef struct hashtable {
	// Current number of elements
	size_t size;
	// Max number of elements
	size_t capacity;
	ht_entry_t** buckets;
	// ht_entry_t->value cleanup function
	void (*free_func)(void*);
	pthread_mutex_t lock;
} hashtable_t;

hashtable_t* ht_init(size_t initial_capacity, void (*free_func)(void*));
void ht_destroy(hashtable_t *ht);
ht_entry_t *ht_put(hashtable_t *ht, const char* key, void* value);
void* ht_get(hashtable_t *ht, const char* key);
void* ht_remove(hashtable_t *ht, const char* key);

#endif // _HASHTABLE_H_
