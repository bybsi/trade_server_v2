#ifndef _RB_TREE_H_
#define _RB_TREE_H_

typedef enum { RED, BLACK } Color;

typedef struct rbt_node_t {
	unsigned long long key;
	DLL_NODE *orders_list;
	Color color;
	struct rbt_node_t *parent;
	struct rbt_node_t *left;
	struct rbt_node_t *right;
} RBT_NODE;

void rbt_inorder(RBT_NODE *node);
void rbt_insert(RBT_NODE **root, unsigned long long key, void *data) {
RBT_NODE *rbt_init();

#endif // _RB_TREE_H_
