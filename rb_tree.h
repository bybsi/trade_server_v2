#ifndef _RB_TREE_H_
#define _RB_TREE_H_

typedef enum { RED, BLACK } Color;

typedef struct rb_node_t {
	int data;
	Color color;
	struct rb_node_t *parent;
	struct rb_node_t *left;
	struct rb_node_t *right;
} RB_NODE;

void rb_inorder(RB_NODE *node);
void rb_insert(RB_NODE **root, int data);
RB_NODE *rb_init();

#endif // _RB_TREE_H_
