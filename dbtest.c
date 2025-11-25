#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "database.h"

int main(int argc, char *argv[]) {
	void *data;
	ST_TBL_TRADE_ORDER *to;

	if (!db_init())
		exit(255);

	data = db_fetch_data(TBL_TRADE_ORDER);
	to = ((ST_TBL_TRADE_ORDER *) data)->next;
	printf("%lu,%u,%s,%c,%llu,%s,%s\n",
		to->id,
		to->user_id,
		to->ticker,
		to->side,
		to->price,
		to->created_at,
		to->filled_at);
	free(data);
	exit(0);
} 
