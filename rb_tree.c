#include <stdio.h>
#include <stdlib.h>

/**
 Red and black tree customized for use with trade service.
 Keys are price points, data is a doubly linked list of
 market orders, ordered by time. A reference to each order
 node is kept in a hashtable for O(1) access in the event of
 cancellation.
 */

// TODO:
//	Comparator
// 	Free function
// 	Currently only allows one instance...
#include "dl_list.h"
#include "rb_tree.h"

static int verbose = 0;
// Global sentinel node for NIL (null) nodes
RBT_NODE *NIL = NULL;

void (*test_print_func) (void *) = NULL;
void rbt_set_test_print_func(void (*ptr) (void *)) {
	test_print_func = ptr;
}

RBT_NODE *rbt_new_node(unsigned long long key, void *data, void (*data_callback)(void *data_node, void *list_node)) {
	RBT_NODE *node = malloc(sizeof(RBT_NODE));
	node->orders_list = dl_list_init(test_print_func);
	dl_list_insert(node->orders_list, data, data_callback);
	node->key = key;
	node->color = RED;
	node->parent = NIL;
	node->left = NIL;
	node->right = NIL;
	return node;
}

static void rotate_left(RBT_NODE **root, RBT_NODE *x) {
	RBT_NODE *y = x->right;
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

static void rotate_right(RBT_NODE **root, RBT_NODE *y) {
	RBT_NODE *x = y->left;
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
static void fix_up(RBT_NODE **root, RBT_NODE *z) {
	while (z->parent->color == RED) {
		if (z->parent == z->parent->parent->left) {
			RBT_NODE *uncle = z->parent->parent->right;
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
			RBT_NODE *uncle = z->parent->parent->left;
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

void rbt_insert(RBT_NODE **root, unsigned long long key, void *data, void (*data_callback)(void *data_node, void *list_node)) {
//	RBT_NODE *new_node = rbt_new_node(key, data);
	RBT_NODE *new_node;
	RBT_NODE *search = NIL;
	RBT_NODE *current = *root;

	while (current != NIL) {
		if (verbose)
			printf("current != NIL (%llu == %llu)\n", key, current->key);
		search = current;
		if (key == current->key) {
			// append to DLL at x->data
			break;
		} else if (key < current->key) {
			current = current->left;
		} else {
			current = current->right;
		}
	}
//	new_node->parent = search;

	new_node = NULL;
	if (search == NIL) {
		if (verbose)
			printf("Creating new node at key: %llu\n", key);
		new_node = rbt_new_node(key, data, data_callback);
		new_node->parent = search;
		*root = new_node;
	} else if (key == search->key) {
		// inserting order with the same price
		if (verbose)
			printf("Updating node at key: %llu\n", key);
		dl_list_insert(search->orders_list, data, data_callback);
	} else if (key < search->key) {
		if (verbose)
			printf("Creating new node (left) at key: %llu\n", key);
		search->left = rbt_new_node(key, data, data_callback);
		search->left->parent = search;
		new_node = search->left;
	} else {
		if (verbose)
			printf("Creating new node (right) at key: %llu\n", key);
		search->right = rbt_new_node(key, data, data_callback);
		search->right->parent = search;
		new_node = search->right;
	}

	if (new_node)
		fix_up(root, new_node);
}

RBT_NODE *rbt_find_nearest(RBT_NODE *node, unsigned long long key) {
	if (!node)
		return NULL;

	if (node->key == key)
		return node;

	if (key < node->key) {
		if (!node->left)
			return node;
		return rbt_find_nearest(node->left, key);
	} else {
		if (!node->right)
			return node;
		return rbt_find_nearest(node->right, key);
	}
}

void rbt_visit_nodes_in_range(RBT_NODE *node, unsigned long long low_key, unsigned long long high_key, void (*visitor) (void *)) {
	if (!node || node == NIL)
		return;
	
	if (node->key > low_key)
		rbt_visit_nodes_in_range(node->left, low_key, high_key, visitor);
	if (node->key >= low_key && node->key <= high_key)
		visitor(node->orders_list);
	if (node->key < high_key)
		rbt_visit_nodes_in_range(node->right, low_key, high_key, visitor);
}
/*
def morris_inorder(root):
	current = root
	while current:
		if not current.left:
			visit
			current = current.right
		else:
			pred = current.left
			while pred.right is not None and pred.right != current:
				pred = pred.right

			if pred.right is None:
				pred.right = current
				cur = cur.right
			else:
				pred.right =  None
				visit
				cur = cur.right

def flatten(root):
	current = root
	while current is not None:
		if current.left:
			pred = current.left
			while pred.right is not None:
				pred = pred.right

			pred.right = current.right
			current.right = current.left
			current.left = None
		current = current.right
*/
void rbt_inorder(RBT_NODE *node) {
	// TODO use morris traversal
	if (node != NIL) {
		DLL_NODE *current; 
		rbt_inorder(node->left);

		printf("\t%llu (%s)\n", node->key, (node->color == RED ? "RED" : "BLACK"));
		current = node->orders_list->head;
		while (current) {
			if (current->data)
				node->orders_list->print_func(current->data);
			current = current->next;
		}
		rbt_inorder(node->right);
	}
}

// TODO destroy function
// TODO delete function

RBT_NODE * rbt_init() {
	if (!NIL) {
		NIL = (RBT_NODE *)malloc(sizeof(RBT_NODE));
		NIL->color = BLACK;
		NIL->left = NULL;
		NIL->right = NULL;
		NIL->parent = NULL;
		NIL->orders_list = NULL;
	}
	return NIL;
}

