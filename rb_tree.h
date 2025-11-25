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

void rbt_inorder(RBT_NODE *node);
void rbt_insert(RBT_NODE **root, unsigned long long key, void *data, void (*data_callback)(void *data_node));
RBT_NODE *rbt_init();

#endif // _RB_TREE_H_
