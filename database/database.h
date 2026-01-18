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

extern char *database_query_str[];
extern char *database_tbl_names[];

void db_timestamp(char *buffer, unsigned short subtract_weeks);
MYSQL_RES * db_fetch_data(DB_TBL_TYPE tbl_type);
MYSQL_RES * db_fetch_data_sql(DB_TBL_TYPE tbl_type, const char *sql);
int db_execute_query(const char *query);
int db_init();
void db_close();

#endif // _DATABASE_H_

