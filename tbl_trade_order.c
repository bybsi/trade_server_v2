
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <time.h>
#include <mysql/mysql.h>
#include "tbl_trade_order.h"

ST_TBL_TRADE_ORDER * parse_tbl_trade_order(MYSQL_RES *result) {
	char *end;
	MYSQL_ROW row;
	ST_TBL_TRADE_ORDER *head, *current; 
	unsigned long tmp_ul;

	if (!result) {
		fprintf(stderr, "Parsing tbl_trade_order (NULL)\n");
		return NULL;
	}

	errno = 0;
	head = malloc(sizeof(ST_TBL_TRADE_ORDER));
	head->next = NULL;
	current = head;
	while ((row = mysql_fetch_row(result)) != NULL) {
		// Build new node
		struct tbl_trade_order *tto = malloc(sizeof(ST_TBL_TRADE_ORDER));
		tto->id      = strtoul(row[0], &end, 10);
		// TODO library function for bounds checking conversion.
		tto->user_id = (unsigned int) strtol(row[1], &end, 10);
		strncpy(tto->ticker, row[2], TICKER_LEN);
		tto->ticker[TICKER_LEN - 1] = '\0';
		tto->side    = row[3] ? *row[3] : '\0';
		tto->status  = row[4] ? *row[4] : '\0';
		tto->type    = row[5] ? *row[5] : '\0';
		tmp_ul       = strtoul(row[6], &end, 10);
		if (tmp_ul <= (unsigned long)UINT_MAX)
			tto->amount = (unsigned int)tmp_ul;
		else
			tto->amount = 0;
		tto->price   = strtoull(row[7], &end, 10);
		strncpy(tto->created_at, row[8] ? row[8] : "", TIMESTAMP_LEN);
		strncpy(tto->filled_at, row[9] ? row[9] : "", TIMESTAMP_LEN);
		tto->created_at[TIMESTAMP_LEN - 1] = '\0';
		tto->filled_at[TIMESTAMP_LEN - 1] = '\0';
		tto->next = NULL;
		// End new node

		current->next = tto;
		current = tto;
	}

	mysql_free_result(result);
	return head;
}

void free_trade_order(ST_TBL_TRADE_ORDER *head) {
	ST_TBL_TRADE_ORDER *next;
	while (head) {
		next = head->next;
		free(head);
		head = next;
	}
}

void print_tbl_trade_order(ST_TBL_TRADE_ORDER *to) {
	printf("%lu,%u,%s,%c,%c,%llu,%s,%s\n",
		to->id,
		to->user_id,
		to->ticker,
		to->side,
		to->status,
		to->price,
		to->created_at,
		to->filled_at);
}


