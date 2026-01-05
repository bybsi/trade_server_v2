#ifndef _RB_TREE_H_
#define _RB_TREE_H_

#include "dl_list.h"

typedef enum { RED, BLACK } ENUM_COLOR;

typedef struct rbt_node {
	unsigned long long key;
	dl_list_t *orders_list;
	ENUM_COLOR color;
	struct rbt_node *parent;
	struct rbt_node *left;
	struct rbt_node *right;
} rbt_node_t;

rbt_node_t * rbt_find_nearest(rbt_node_t *node, unsigned long long key);
void rbt_visit_nodes_in_range(rbt_node_t *node, unsigned long long low_key, unsigned long long high_key, void (*visitor) (void *));
void rbt_inorder(rbt_node_t *node);
void rbt_insert(rbt_node_t **root, unsigned long long key, void *data, void (*data_callback)(void *data_node, void *list_node));
rbt_node_t * rbt_init();
void rbt_set_test_print_func(void (*ptr) (void *));

#endif // _RB_TREE_H_
