#ifndef _TBL_TRADE_ORDER_H
#define _TBL_TRADE_ORDER_H

#include <mysql/mysql.h>
#include "database.h"

typedef struct tbl_trade_order {
	unsigned long id;
	unsigned int user_id;
	char ticker[TICKER_LEN];
	// [B]uy, [S]ell
	char side;
	// [F]illed, [O]pen, [X]anceled
	char status;
	// [L]imit, [M]arket
	char type;
	unsigned int amount;
	unsigned long long price;
	// YYYY-MM-DD HH:MM:SS
	char created_at[TIMESTAMP_LEN];
	char filled_at[TIMESTAMP_LEN];

	struct tbl_trade_order *next;
} st_tbl_trade_order_t;

st_tbl_trade_order_t * parse_tbl_trade_order(MYSQL_RES *result);
void free_trade_order(st_tbl_trade_order_t *head);
void print_tbl_trade_order(st_tbl_trade_order_t *to);

#endif
