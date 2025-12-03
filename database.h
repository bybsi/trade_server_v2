#ifndef _DATABASE_H_
#define _DATABASE_H_

#include <mysql/mysql.h>

// ANDTHEN, FORIS4, SPARK, ZILBIAN ...
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

static char *database_tbl_names[] = {
	"tbl_trade_orders",
	"tbl_user_currency"
};
static char *database_query_str[] = {
	"SELECT * FROM tbl_trade_orders",
	"SELECT * FROM tbl_user_currency"
};

void db_timestamp(char *buffer, unsigned short subtract_weeks);
void *db_fetch_data(DB_TBL_TYPE tbl_type);
void *db_fetch_data_sql(DB_TBL_TYPE tbl_type, const char *sql);
int db_execute_query(const char *query);
int db_init();
void db_close();

#endif // _DATABASE_H_

