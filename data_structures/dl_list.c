/*
QUEUE

Doubly linked list that behaves like a queue.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dl_list.h"
#include "error.h"

/*
Initializes the list.

Params
	print_func: Optional function used to output the data of a node.

Returns
	A pointer to the list or NULL if allocation fails.
*/
dl_list_t * dl_list_init(void (*print_func) (void*)) {
	dl_list_t *dl_list = malloc(sizeof(dl_list_t));
	if (!dl_list) {
		fprintf(stderr, "Could not allocate dl_list\n");
		return NULL;
	}
	dl_list->head = dl_list_new_node(NULL);
	dl_list->tail = dl_list->head;
	dl_list->iter = dl_list->head;
	dl_list->print_func = print_func;
	return dl_list;
}

/*
Allocates a new list node and sets its data.

Params
	data: the data to store in the node.

Returns
	A pointer to the list node or NULL if allocation fails.
*/
dll_node_t * dl_list_new_node(void *data) {
	dll_node_t *node = malloc(sizeof(dll_node_t));
	if (!node) {
		fprintf(stderr, "Could not allocate dl_list node\n");
		return NULL;
	}
	node->data = data;
	node->prev = NULL;
	node->next = NULL;
	return node;
}

/*
Inserts an element at the END of the list, executing an optional callback.

Params
	dl_list: A pointer to the list instance.
	data: A pointer to the data to be inserted.
	data_callback: Optional callback function.
*/
void dl_list_insert(dl_list_t *dl_list, void *data, void (*data_callback) (void *, void *)) {
	if (!dl_list)
		return;

	dll_node_t *new_node = dl_list_new_node(data);
	new_node->prev = dl_list->tail;
	dl_list->tail->next = new_node;
	dl_list->tail = new_node;
	if (data_callback) 
		data_callback(data, new_node);
}

void dl_list_remove(dl_list_t *dl_list, dll_node_t *node) {
	// Currently the caller free's the data from the node,
	// it is also pointed to by the hashtable.
	if (!node)
		return;

	if (node == dl_list->tail) {
		dl_list->tail = dl_list->tail->prev;
		dl_list->tail->next = NULL;
	} else {
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}
	free(node);
}

void dl_list_destroy(dl_list_t *dl_list) {
	dll_node_t *cur, *next;
	
	cur = dl_list->head;
	while (cur) {
		next = cur->next;
		free(cur);
		cur = next;
	}
	free(dl_list);
}

