#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dl_list.h"
DL_LIST * dl_list_init(void (*print_func) (void*)) {
	DL_LIST *dl_list = malloc(sizeof(DL_LIST));
	dl_list->head = dl_list_new_node(NULL);
	dl_list->tail = dl_list->head;
	dl_list->iter = dl_list->head;
	dl_list->print_func = print_func;
	return dl_list;
}

DLL_NODE * dl_list_new_node(void *data) {
	DLL_NODE *node = malloc(sizeof(DLL_NODE));
	node->data = data;
	node->prev = NULL;
	node->next = NULL;
	return node;
}

void dl_list_insert(DL_LIST *dl_list, void *data, void (*data_callback) (void *, void *)) {
	DLL_NODE *new_node = dl_list_new_node(data);
	new_node->prev = dl_list->tail;
	dl_list->tail->next = new_node;
	dl_list->tail = new_node;
	if (data_callback) {
		//data_callback(HT_ENTRY *, DLL_NODE *);
		data_callback(data, new_node);
	}
}


