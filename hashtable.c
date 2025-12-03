#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "hashtable.h"

#define INITIAL_CAPACITY 16
#define RESIZE_THRESHOLD 0.75

/*
ELF hash.
*/
static size_t hash_function(const char* key, size_t capacity) {
	size_t hash = 0;
	size_t high;

	while (*key) {
		hash = (hash << 4) + *key++;
		high = hash & 0xF0000000;
		if (high) {
			hash ^= high >> 24;
			hash &= ~high;
		}
	}
	return hash % capacity;
}

static HT_ENTRY* create_entry(const char* key, void* value) {
	HT_ENTRY* entry = malloc(sizeof(HT_ENTRY));
	if (!entry) 
		return NULL;

	entry->key = strdup(key);
	if (!entry->key) {
		free(entry);
		return NULL;
	}

	entry->value = value;
	// Collision list
	entry->next = NULL;
	return entry;
}

static int resize_hashtable(HASHTABLE *ht, size_t new_capacity) {
	size_t i, new_index;
	HT_ENTRY** new_buckets = calloc(new_capacity, sizeof(HT_ENTRY*));
	if (!new_buckets) 
		return 0;

	for (i = 0; i < ht->capacity; i++) {
		if (!ht->buckets[i])
			continue;
		new_index =  hash_function(ht->buckets[i]->key, new_capacity);
		new_buckets[new_index] = ht->buckets[i];
	}

	free(ht->buckets);
	ht->buckets = new_buckets;
	ht->capacity = new_capacity;
	return 1;
}

HASHTABLE* ht_init(size_t initial_capacity, void (*free_func)(void*)) {
	if (initial_capacity == 0) 
		initial_capacity = INITIAL_CAPACITY;

	HASHTABLE* ht = malloc(sizeof(HASHTABLE));
	if (!ht) 
		return NULL;

	ht->buckets = calloc(initial_capacity, sizeof(HT_ENTRY*));
	if (!ht->buckets) {
		free(ht);
		return NULL;
	}

	if (pthread_mutex_init(&ht->lock, NULL) != 0) {
		free(ht->buckets);
		free(ht);
		return NULL;
	}

	ht->size = 0;
	ht->capacity = initial_capacity;
	ht->free_func = free_func;
	return ht;
}

void ht_destroy(HASHTABLE* ht) {
	size_t i;
	if (!ht) 
		return;

	pthread_mutex_lock(&ht->lock);

	for (i = 0; i < ht->capacity; i++) {
		if (!ht->buckets[i])
			continue;

		if (ht->free_func)
			ht->free_func(ht->buckets[i]->value);
		free(ht->buckets[i]->key);
		free(ht->buckets[i]);
	}
	free(ht->buckets);
	
	pthread_mutex_unlock(&ht->lock);
	pthread_mutex_destroy(&ht->lock);
	
	free(ht);
}

HT_ENTRY * ht_put(HASHTABLE* ht, const char* key, void* value) {
	if (!ht || !key) 
		return NULL;

	pthread_mutex_lock(&ht->lock);

	if ((float)(ht->size + 1) / ht->capacity > RESIZE_THRESHOLD) {
		if (!resize_hashtable(ht, ht->capacity * 2)) {
			pthread_mutex_unlock(&ht->lock);
			return NULL;
		}
	}

	size_t index = hash_function(key, ht->capacity);
	// TODO this is just ht_get;
	HT_ENTRY* entry = ht->buckets[index];

	while (entry) {
		if (strcmp(entry->key, key) == 0) {
			if (ht->free_func) 
				ht->free_func(entry->value);
			entry->value = value;
			pthread_mutex_unlock(&ht->lock);
			return entry;
		}
		entry = entry->next;
	}

	HT_ENTRY* new_entry = create_entry(key, value);
	if (!new_entry) {
		pthread_mutex_unlock(&ht->lock);
		return NULL;
	}

	new_entry->next = ht->buckets[index];
	ht->buckets[index] = new_entry;

	ht->size++;
	pthread_mutex_unlock(&ht->lock);
	return new_entry;
}

void* ht_get(HASHTABLE* ht, const char* key) {
	if (!ht || !key) 
		return NULL;

	pthread_mutex_lock(&ht->lock);
	
	size_t index = hash_function(key, ht->capacity);
	HT_ENTRY* entry = ht->buckets[index];
	void* result = NULL;

	while (entry) {
		if (strcmp(entry->key, key) == 0) {
			result = entry->value;
			break;
		}
		entry = entry->next;
	}

	pthread_mutex_unlock(&ht->lock);
	return result;
}

void* ht_remove(HASHTABLE* ht, const char* key) {
	if (!ht || !key) 
		return NULL;

	pthread_mutex_lock(&ht->lock);

	size_t index = hash_function(key, ht->capacity);
	HT_ENTRY* entry = ht->buckets[index];
	HT_ENTRY* prev = NULL;
	void* result = NULL;

	while (entry) {
		if (strcmp(entry->key, key) == 0) {
			if (prev) 
				prev->next = entry->next;
			else 
				ht->buckets[index] = entry->next;

			result = entry->value;
			free(entry->key);
			free(entry);
			ht->size--;
			break;
		}
		prev = entry;
		entry = entry->next;
	}

	pthread_mutex_unlock(&ht->lock);
	return result;
}

