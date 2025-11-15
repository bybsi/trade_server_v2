#include "linked_hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define INITIAL_CAPACITY 16
#define RESIZE_THRESHOLD 0.75

// ELF hash
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

static lhm_node* create_entry(const char* key, void* value) {
    lhm_node* entry = malloc(sizeof(lhm_node));
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
    entry->list_prev = NULL;
    entry->list_next = NULL;
    return entry;
}

static int resize_map(lhmap* map, size_t new_capacity) {
    lhm_node** new_buckets = calloc(new_capacity, sizeof(lhm_node*));
    if (!new_buckets) 
    	return 0;

    lhm_node* entry = map->head;
    while (entry) {
        entry->next = NULL;
        entry = entry->list_next;
    }

    entry = map->head;
    while (entry) {
        size_t index = hash_function(entry->key, new_capacity);
        entry->next = new_buckets[index];
        new_buckets[index] = entry;
        entry = entry->list_next;
    }

    free(map->buckets);
    map->buckets = new_buckets;
    map->capacity = new_capacity;
    return 1;
}

lhmap* lhmap_init(size_t initial_capacity, void (*free_func)(void*)) {
    if (initial_capacity == 0) 
    	initial_capacity = INITIAL_CAPACITY;

    lhmap* map = malloc(sizeof(lhmap));
    if (!map) 
    	return NULL;

    map->buckets = calloc(initial_capacity, sizeof(lhm_node*));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    if (pthread_mutex_init(&map->lock, NULL) != 0) {
        free(map->buckets);
        free(map);
        return NULL;
    }

    map->size = 0;
    map->capacity = initial_capacity;
    map->head = NULL;
    map->tail = NULL;
    map->free_func = free_func;
    return map;
}

void lhmap_destroy(lhmap* map) {
    if (!map) 
    	return;

    pthread_mutex_lock(&map->lock);

    lhm_node* entry = map->head;
    while (entry) {
        lhm_node* next = entry->list_next;
        if (map->free_func) 
		map->free_func(entry->value);
        free(entry->key);
        free(entry);
        entry = next;
    }

    free(map->buckets);
    
    pthread_mutex_unlock(&map->lock);
    pthread_mutex_destroy(&map->lock);
    
    free(map);
}

int lhmap_put(lhmap* map, const char* key, void* value) {
    if (!map || !key) 
    	return 0;

    pthread_mutex_lock(&map->lock);

    if ((float)(map->size + 1) / map->capacity > RESIZE_THRESHOLD) {
        if (!resize_map(map, map->capacity * 2)) {
            pthread_mutex_unlock(&map->lock);
            return 0;
        }
    }

    size_t index = hash_function(key, map->capacity);
    lhm_node* entry = map->buckets[index];

    while (entry) {
    	// We need to allow duplicate keys right?
	// price 1000.58, 1000.48 for example
	// or price 1000 quantity 123, price 1000 quantity 22.
        if (strcmp(entry->key, key) == 0) {
            if (map->free_func) 
	    	map->free_func(entry->value);
            entry->value = value;
            pthread_mutex_unlock(&map->lock);
            return 1;
        }
        entry = entry->next;
    }

    lhm_node* new_entry = create_entry(key, value);
    if (!new_entry) {
        pthread_mutex_unlock(&map->lock);
        return 0;
    }

    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;

    if (!map->head) {
        map->head = new_entry;
        map->tail = new_entry;
    } else {
        new_entry->list_prev = map->tail;
        map->tail->list_next = new_entry;
        map->tail = new_entry;
    }

    map->size++;
    pthread_mutex_unlock(&map->lock);
    return 1;
}

void* lhmap_get(lhmap* map, const char* key) {
    if (!map || !key) 
    	return NULL;

    pthread_mutex_lock(&map->lock);
    
    size_t index = hash_function(key, map->capacity);
    lhm_node* entry = map->buckets[index];
    void* result = NULL;

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            result = entry->value;
            break;
        }
        entry = entry->next;
    }

    pthread_mutex_unlock(&map->lock);
    return result;
}

void* lhmap_remove(lhmap* map, const char* key) {
    if (!map || !key) 
    	return NULL;

    pthread_mutex_lock(&map->lock);

    size_t index = hash_function(key, map->capacity);
    lhm_node* entry = map->buckets[index];
    lhm_node* prev = NULL;
    void* result = NULL;

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev) 
	    	prev->next = entry->next;
            else 
	    	map->buckets[index] = entry->next;

            if (entry->list_prev) 
	    	entry->list_prev->list_next = entry->list_next;
            else 
	    	map->head = entry->list_next;

            if (entry->list_next) 
	    	entry->list_next->list_prev = entry->list_prev;
            else 
	    	map->tail = entry->list_prev;

            result = entry->value;
            free(entry->key);
            free(entry);
            map->size--;
            break;
        }
        prev = entry;
        entry = entry->next;
    }

    pthread_mutex_unlock(&map->lock);
    return result;
}

size_t lhmap_size(const lhmap* map) {
    if (!map) 
    	return 0;
    
    pthread_mutex_lock(&((lhmap*)map)->lock);
    size_t size = map->size;
    pthread_mutex_unlock(&((lhmap*)map)->lock);
    
    return size;
}

int lhmap_contains_key(const lhmap* map, const char* key) {
    if (!map || !key) 
    	return 0;
    
    pthread_mutex_lock(&((lhmap*)map)->lock);
    
    size_t index = hash_function(key, map->capacity);
    lhm_node* entry = map->buckets[index];
    int result = 0;

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            result = 1;
            break;
        }
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&((lhmap*)map)->lock);
    return result;
}

lhm_node* lhmap_first(const lhmap* map) {
    return map ? map->head : NULL;
}

lhm_node* lhmap_next(const lhm_node* entry) {
    return entry ? entry->list_next : NULL;
}

const char* lhmap_entry_key(const lhm_node* entry) {
    return entry ? entry->key : NULL;
}

void* lhmap_entry_value(const lhm_node* entry) {
    return entry ? entry->value : NULL;
} 
