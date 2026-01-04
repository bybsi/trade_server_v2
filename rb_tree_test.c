#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//#include "dl_list.h"
#include "rb_tree.h"
#include "database.h"

void pf(void *data) {
	print_tbl_trade_order((ST_TBL_TRADE_ORDER *) data);
}

void visit_list(void *list) {
	DL_LIST *dll = (DL_LIST *) list;
	DLL_NODE *cur = dll->head;

	while (cur) {
		if (cur->data)
			print_tbl_trade_order((ST_TBL_TRADE_ORDER *)cur->data);
		cur = cur->next;
	}
}

int main() {
	ST_TBL_TRADE_ORDER *to_head, *to;
	RBT_NODE *root;
	RBT_NODE *root1;
	RBT_NODE *node;

	if (!db_init())
		exit(255);

	root = rbt_init();
	root1 = rbt_init();
	rbt_set_test_print_func(&pf);

	to_head = parse_tbl_trade_order( db_fetch_data(TBL_TRADE_ORDER) );
	if (!to_head) {
		printf("no data\n");
		exit(1);
	}
	
	for (to = to_head->next; to; to = to->next) {
		print_tbl_trade_order(to);
		rbt_insert(&root, to->price, to, NULL);
		rbt_insert(&root1, to->price + 10, to, NULL);
	}

	rbt_inorder(root);
	printf("\n\n");
	rbt_inorder(root1);

	printf("outputting nodes from 0 -> 13\n");
	rbt_visit_nodes_in_range(root, 0, 13, &visit_list);
	printf("\n\n");
	
	printf("outputting nodes from 2 -> 3\n");
	rbt_visit_nodes_in_range(root, 2, 3, &visit_list);
	printf("\n\n");
	

	free_trade_order(to_head);
	
	db_close();

	exit(0);
}

