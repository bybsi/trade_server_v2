#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//#include "dl_list.h"
#include "rb_tree.h"
#include "database.h"

static unsigned short load_buy_orders() {
	ST_TBL_TRADE_ORDER *tbl_trade_order;
	static char sql[1024];
	snprintf(sql, 1024, 
"WHERE ticker='ANDTHEN' and side='B' and status='O'"
"ORDER BY price ASC, created_at ASC ");
	tbl_trade_order = (ST_TBL_TRADE_ORDER *) db_fetch_data_sql(TBL_TRADE_ORDER, sql);
	
}

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
	void *data;
	ST_TBL_TRADE_ORDER *to;
	RBT_NODE *root;
	RBT_NODE *root1;
	RBT_NODE *node;

	if (!db_init())
		exit(255);

	root = rbt_init();
	root1 = rbt_init();
	rbt_set_test_print_func(&pf);

	data = db_fetch_data(TBL_TRADE_ORDER);
	to = ((ST_TBL_TRADE_ORDER *) data)->next;

	while (to) {
		print_tbl_trade_order(to);
		rbt_insert(&root, to->price, to, NULL);
		rbt_insert(&root1, to->price + 10, to, NULL);
		to = to->next;
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
	

	return 0;
}

