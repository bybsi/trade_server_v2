#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mysql/mysql.h>

#include "database.h"

// TODO Decrypt PW like in the Python version.
// 	Also use environment variables for this with getenv().
//  - currently plaintext in .dbconnect file
#define DB_CONNECT_STR(var) var[0], var[1], var[2], var[3]

// Global MySQL connection
MYSQL *mysql_conn = NULL;

int db_init() {
	FILE *fh;
	char buffer[32];
	unsigned short line_num, len;
	char *mysql_connect_vars[4];

	mysql_conn = mysql_init(NULL);
	if (mysql_conn == NULL) {
		fprintf(stderr, "Failed to initialize MySQL: %s\n", mysql_error(mysql_conn));
		return 0;
	}

	if (!(fh = fopen(".dbconnect", "r"))) {
		fprintf(stderr, "Failed to open database connection file.\n");
		return 0;
	}

	line_num = 0;
	while (line_num < 4 && fgets(buffer, sizeof(buffer), fh) != NULL)  {
		len = strlen(buffer);
		mysql_connect_vars[line_num] = strdup(buffer);
		if (buffer[len - 1] == '\n')
			mysql_connect_vars[line_num][len - 1] = '\0';
		line_num++;
	}
	fclose(fh);

	if (mysql_real_connect(mysql_conn, DB_CONNECT_STR(mysql_connect_vars), 0, NULL, 0) == NULL) {
		fprintf(stderr, "Failed to connect to MySQL: %s\n", mysql_error(mysql_conn));
		mysql_close(mysql_conn);
		mysql_conn = NULL;
		return 0;
	}

	return 1;
}

void db_close() {
	if (mysql_conn) {
		mysql_close(mysql_conn);
	}
}

int db_execute_query(const char *query) {
	if (!mysql_conn) {
		fprintf(stderr, "MySQL not connected\n");
		return -1;
	}

	if (mysql_query(mysql_conn, query) != 0) {
		fprintf(stderr, "Query failed: %s\n", mysql_error(mysql_conn));
		return -1;
	}

	return 0;
}

// Parse functions are defined in each tbl_*.c file.
// The order of this array must match the DB_TBL_TYPE enum order.
typedef void * (*tbl_mapper)(MYSQL_RES *);
tbl_mapper tbl_map[] = { 
	parse_tbl_trade_order,
	parse_tbl_user_currency
};
// Returns a linked list of SQL records.
// Client must cast the returned node to the appropriate type.
void* db_fetch_data(DB_TBL_TYPE tbl_type) {
	void *data;
	MYSQL_RES *result;

	if (!mysql_conn) 
		return NULL;

//	const char *query = "SELECT data FROM events ORDER BY timestamp DESC LIMIT 1";
	if (mysql_query(mysql_conn, database_query_str[tbl_type]) != 0) {
		fprintf(stderr, "Query failed: %s\n", mysql_error(mysql_conn));
		return NULL;
	}

	result = mysql_store_result(mysql_conn);
	if (!result) {
		fprintf(stderr, "Failed to store result: %s\n", mysql_error(mysql_conn));
		return NULL;
	}

	// Convert the raw SQL string data into the struct representation
	// of the database table.
	data = tbl_map[tbl_type](result);

	mysql_free_result(result);
	return data;
}
