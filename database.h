#ifndef _DATABASE_H_
#define _DATABASE_H_

#include <mysql/mysql.h>
#include "tbl_trade_order.h"
#include "tbl_user_currency.h"

// Global MySQL connection
MYSQL *mysql_conn = NULL;

typedef enum {
	TBL_TRADE_ORDER = 0,
	TBL_USER_CURRENCY
} DB_TBL_TYPE;

void* db_fetch_data(const char *query, DB_TBL_TYPE tbl_type) {
int db_execute_query(const char *query);
int db_init();
void db_close();

#endif // _DATABASE_H_

