#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "database.h"

int main(int argc, char *argv[]) {
	void *data;
	ST_TBL_TRADE_ORDER *to_head, *to;
	ST_TBL_USER_CURRENCY *uc_head, *uc;

	if (!db_init())
		exit(255);

	to_head = parse_tbl_trade_order( db_fetch_data(TBL_TRADE_ORDER) );
	if (!to_head) {
		printf("no data\n");
		exit(1);
	}
	
	for (to = to_head->next; to; to = to->next) 
		printf("%lu,%u,%s,%c,%llu,%s,%s\n",
			to->id,
			to->user_id,
			to->ticker,
			to->side,
			to->price,
			to->created_at,
			to->filled_at);

	free_trade_order(to_head);

	uc_head = parse_tbl_user_currency( db_fetch_data(TBL_USER_CURRENCY) );
	if (!uc_head) {
		printf("no uc data\n");
		exit(2);
	}
	for (uc = uc_head->next; uc; uc = uc->next)
		printf("%u,%llu,%llu,%llu,%llu,%llu\n",
			uc->user_id,
			uc->bybs,
			uc->andthen, 
			uc->foris4,  
			uc->zilbian,
			uc->spark);

	free_user_currency(uc_head);

	db_close();

	exit(0);
} 
