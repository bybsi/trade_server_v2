#ifndef _LINKED_HASHMAP_H_
#define _LINKED_HASHMAP_H_

#include <stddef.h>
#include <pthread.h>

typedef struct lhm_node {
	char* key;
	void* value;
	lhm_node* next;	   // hash collision chain
	lhm_node* list_prev;
	lhm_node* list_next;
} lhm_node;

typedef struct lhmap {
	// Num elements
	size_t size;
	// Max elements
	size_t capacity;
	lhm_node** buckets;
	lhm_node* head;
	lhm_node* tail;
	// Node cleanup function
	void (*free_func)(void*);
	pthread_mutex_t lock;
} lhmap;

lhmap* lhmap_init(size_t initial_capacity, void (*free_func)(void*));
void lhmap_destroy(lhmap* map);
int lhmap_put(lhmap* map, const char* key, void* value);
void* lhmap_get(lhmap* map, const char* key);
void* lhmap_remove(lhmap* map, const char* key);
size_t lhmap_size(const lhmap* map);
int lhmap_contains_key(const lhmap* map, const char* key);
lhm_node* lhmap_first(const lhmap* map);
lhm_node* lhmap_next(const lhm_node* entry);
const char* lhmap_entry_key(const lhm_node* entry);
void* lhmap_entry_value(const lhm_node* entry);

#endif // _LINKED_HASHMAP_H_
