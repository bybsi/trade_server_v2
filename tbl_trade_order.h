#ifndef _TBL_TRADE_ORDER_H
#define _TBL_TRADE_ORDER_H

#include <mysql/mysql.h>

struct tbl_trade_order {
	unsigned long id;
	unsigned int user_id;
	char ticker[8];
	// [B]uy, [S]ell
	char side;
	// [F]illed, [O]pen, [X]anceled
	char status;
	// [L]imit, [M]arket
	char type;
	unsigned int amount;
	unsigned long long price;
	// YYYY-MM-DD HH:MM:SS
	char created_at[19];
	char filled_at[19];

	struct tbl_trade_order *next;
};

void * parse_tbl_trade_order(MYSQL_RES *result);


#endif
