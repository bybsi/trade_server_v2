#ifndef _DATABASE_H_
#define _DATABASE_H_

#include <mysql/mysql.h>

#ifndef TICKER_LEN
#define TICKER_LEN 8
#endif
#ifndef TIMESTAMP_LEN
// YYYY-MM-DD HH:MM:SS
#define TIMESTAMP_LEN 20
#endif

#include "tbl_user_currency.h"
#include "tbl_trade_order.h"

typedef enum {
	TBL_TRADE_ORDER = 0,
	TBL_USER_CURRENCY
} DB_TBL_TYPE;

static char *database_query_str[] = {
	"SELECT * FROM tbl_trade_orders",
	"SELECT * FROM tbl_user_currency"
};

void* db_fetch_data(DB_TBL_TYPE tbl_type);
int db_execute_query(const char *query);
int db_init();
void db_close();

#endif // _DATABASE_H_

