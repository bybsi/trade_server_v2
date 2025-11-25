#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

int main() {
	void *data;
	ST_TBL_TRADE_ORDER *to;
	RBT_NODE *root;
	RBT_NODE *root1;

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
	return 0;
}

