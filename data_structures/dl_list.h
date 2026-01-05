#ifndef _dl_list_t_H_
#define _dl_list_t_H_

typedef struct dll_node {
	void *data;
	struct dll_node *prev;
	struct dll_node *next;
} dll_node_t;

typedef struct dl_list_t {
	dll_node_t *head;
	dll_node_t *tail;
	dll_node_t *iter;
	void (*print_func) (void *);
} dl_list_t;


// TODO iterator
dl_list_t * dl_list_init(void (*print_func) (void *));
dll_node_t *dl_list_new_node(void *data);
void dl_list_insert(dl_list_t *dl_list, void *data, void (*data_callback)(void *data_node, void *dll_node));
void dl_list_remove(dl_list_t *dl_list, dll_node_t *node);

#endif // _dl_list_t_H__
