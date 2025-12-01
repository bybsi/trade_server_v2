#ifndef _RB_TREE_H_
#define _RB_TREE_H_

#include "dl_list.h"

typedef enum { RED, BLACK } ENUM_COLOR;

typedef struct rbt_node_t {
	unsigned long long key;
	DL_LIST *orders_list;
	ENUM_COLOR color;
	struct rbt_node_t *parent;
	struct rbt_node_t *left;
	struct rbt_node_t *right;
} RBT_NODE;

RBT_NODE *rbt_find_nearest(RBT_NODE *node, unsigned long long key);
//
void rbt_visit_nodes_in_range(RBT_NODE *node, unsigned long long low_key, unsigned long long high_key, void (*visitor) (void *));
//
void rbt_inorder(RBT_NODE *node);
void rbt_insert(RBT_NODE **root, unsigned long long key, void *data, void (*data_callback)(void *data_node, void *list_node));
RBT_NODE *rbt_init();
void rbt_set_test_print_func(void (*ptr) (void *));

#endif // _RB_TREE_H_
