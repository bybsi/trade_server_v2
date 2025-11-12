
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <time.h>
#include <mysql/mysql.h>
#include "tbl_trade_order.h"

#ifndef TICKER_LEN
#define TICKER_LEN 8
#endif
#ifndef TIMESTAMP_LEN
// YYYY-MM-DD HH:MM:SS
#define TIMESTAMP_LEN 20
#endif

void * parse_tbl_trade_order(MYSQL_RES *result) {
	char *end;
	MYSQL_ROW row;
	struct tbl_trade_order *head, *current; 
	unsigned long tmpul;
	
	errno = 0;
	head = malloc(sizeof(TBL_TRADE_ORDER));
	head->next = NULL;
	current = head;
	while ((row = mysql_fetch_row(result)) != NULL) {
		// Build new node
		struct tbl_trade_order *tto = malloc(sizeof(TBL_TRADE_ORDER));
		tto->id      = strtoul(row[0], &end, 10);
		tto->user_id = strtoi(row[1], &end, 10);
		strncpy(tto->ticker, row[2], TICKER_LEN);
		tto-ticker[TICKER_LEN - 1] = '\0';
		tto->side    = *row[3];
		tto->status  = *row[4];
		tto->type    = *row[5];
		tmp_ul       = strtoul(row[6], &end, 10);
		if (tmp_ul <= (unsigned long)UINT_MAX)
			tto->amount = (unsigned int)tmp_ul;
		else
			tto->amount = 0;
		tto->price   = strtoull(row[7], &end, 10);
		strncpy(tto->created_at, row[8], TIMESTAMP_LEN);
		strncpy(tto->filled_at, row[9], TIMESTAMP_LEN);
		tto->created_at[TIMESTAMP_LEN - 1] = '\0';
		tto->filled_at[TIMESTAMP_LEN - 1] = '\0';
		tto->next = NULL;
		// End new node

		current->next = tto;
		current = tto;
	}
	return (*void) head;
}

