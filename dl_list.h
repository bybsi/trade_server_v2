#ifndef _DL_LIST_H_
#define _DL_LIST_H_

typedef struct dl_list_node_t {
	void *data;
	struct dl_list_node_t *prev;
	struct dl_list_node_t *next;
} DLL_NODE;

typedef struct dl_list_t {
	DLL_NODE *head;
	DLL_NODE *tail;
	DLL_NODE *iter;
	void (*print_func) (void *);
} DL_LIST;


// TODO iterator
DL_LIST * dl_list_init(void (*print_func) (void *));
DLL_NODE *dl_list_new_node(void *data);
void dl_list_insert(DL_LIST *dl_list, void *data, void (*data_callback)(void *data_node, void *dll_node));

#endif // _DL_LIST_H__
