#include <stdio.h>
#include <stdlib.h>

// TODO:
//	Comparator
// 	Free function
// 	Currently only allows one instance...
#include "rb_tree.h"

// Global sentinel node for NIL (null) nodes
RB_NODE *NIL;

RB_NODE *new_rb_node(int data) {
	RB_NODE *node = (RB_NODE *)malloc(sizeof(RB_NODE));
	node->data = data;
	node->color = RED;
	node->parent = NIL;
	node->left = NIL;
	node->right = NIL;
	return node;
}

static void rotate_left(RB_NODE **root, RB_NODE *x) {
	RB_NODE *y = x->right;
	x->right = y->left;
	if (y->left != NIL) {
		y->left->parent = x;
	}
	y->parent = x->parent;
	if (x->parent == NIL) {
		*root = y;
	} else if (x == x->parent->left) {
		x->parent->left = y;
	} else {
		x->parent->right = y;
	}
	y->left = x;
	x->parent = y;
}

static void rotate_right(RB_NODE **root, RB_NODE *y) {
	RB_NODE *x = y->left;
	y->left = x->right;
	if (x->right != NIL) {
		x->right->parent = y;
	}
	x->parent = y->parent;
	if (y->parent == NIL) {
		*root = x;
	} else if (y == y->parent->left) {
		y->parent->left = x;
	} else {
		y->parent->right = x;
	}
	x->right = y;
	y->parent = x;
}

// Function to fix Red-Black Tree properties after insertion
static void fix_up(RB_NODE **root, RB_NODE *z) {
	while (z->parent->color == RED) {
		if (z->parent == z->parent->parent->left) {
			RB_NODE *uncle = z->parent->parent->right;
			if (uncle->color == RED) {
				// Uncle is red
				z->parent->color = BLACK;
				uncle->color = BLACK;
				z->parent->parent->color = RED;
				z = z->parent->parent;
			} else {
				// Uncle is black, z is right child
				if (z == z->parent->right) {
					z = z->parent;
					rotate_left(root, z);
				}
				// Uncle is black, z is left child
				z->parent->color = BLACK;
				z->parent->parent->color = RED;
				rotate_right(root, z->parent->parent);
			}
		} else {
			// Symmetric cases for when parent is right child
			RB_NODE *uncle = z->parent->parent->left;
			if (uncle->color == RED) {
				// Uncle is red
				z->parent->color = BLACK;
				uncle->color = BLACK;
				z->parent->parent->color = RED;
				z = z->parent->parent;
			} else {
				// Uncle is black, z is left child
				if (z == z->parent->left) {
					z = z->parent;
					rotate_right(root, z);
				}
				// Uncle is black, z is right child
				z->parent->color = BLACK;
				z->parent->parent->color = RED;
				rotate_left(root, z->parent->parent);
			}
		}
	}
	(*root)->color = BLACK;
}

void rb_insert(RB_NODE **root, int data) {
	RB_NODE *new_node = new_rb_node(data);
	RB_NODE *y = NIL;
	RB_NODE *x = *root;

	while (x != NIL) {
		y = x;
		// TODO comparator
		if (new_node->data < x->data) {
			x = x->left;
		} else {
			x = x->right;
		}
	}
	new_node->parent = y;
	if (y == NIL) {
		*root = new_node;
		// TODO Comparator
	} else if (new_node->data < y->data) {
		y->left = new_node;
	} else {
		y->right = new_node;
	}

	fix_up(root, new_node);
}

void rb_inorder(RB_NODE *node) {
	if (node != NIL) {
		rb_inorder(node->left);
		printf("%d (%s) ", node->data, (node->color == RED ? "RED" : "BLACK"));
		rb_inorder(node->right);
	}
}

RB_NODE * rb_init() {
    NIL = (RB_NODE *)malloc(sizeof(RB_NODE));
    NIL->color = BLACK;
    NIL->left = NULL;
    NIL->right = NULL;
    NIL->parent = NULL;
    return NIL;
}
/*
int main() {

    RB_NODE *root = rb_init();

    rb_insert(&root, 10);
    rb_insert(&root, 20);
    rb_insert(&root, 30);
    rb_insert(&root, 15);
    rb_insert(&root, 25);

    rb_inorder(root);
    printf("\n");

    free(root);

    return 0;
}
*/
