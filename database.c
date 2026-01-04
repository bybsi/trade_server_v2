#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mysql/mysql.h>

#include "database.h"

// TODO Decrypt PW from .keys dir.
// 	Also use environment variables via getenv().
//  - currently plaintext in .dbconnect file
#define DB_CONNECT_STR(var) var[0], var[1], var[2], var[3]

// Single connection for now.
MYSQL *mysql_conn = NULL;

/*
Reads the .dbconnect file format:
HOSTADDR
USERNAME
PASSWORD
DBNAME

Params
	vars: The read values will be stored here.

Returns 
	1 on success, 0 on failure.
*/	
unsigned short read_mysql_connect_file(char vars[][32]) {
	FILE *fh;
	char buffer[32];
	unsigned short line_idx = -1, len = 0;
	
	if (!(fh = fopen(".dbconnect", "r"))) {
		fprintf(stderr, "Failed to open database connection file.\n");
		return 0;
	}

	while (++line_idx < 4 && fgets(buffer, sizeof(buffer), fh) != NULL)  {
		len = strlen(buffer);
		strncpy(vars[line_idx], buffer, 32);
		if (buffer[len - 1] == '\n')
			vars[line_idx][len - 1] = '\0';
	}
	fclose(fh);

	if (line_idx != 4) {
		fprintf(stderr, "Invalid number of lines in database connection file.\n");
		return 0;
	}

	return 1;
}

/*
Initializes the single MySQL connection.

Returns
	1 on succes, 0 on failure.
*/
int db_init() {
	char mysql_connect_vars[4][32];

	mysql_conn = mysql_init(NULL);
	if (mysql_conn == NULL) {
		fprintf(stderr, "Failed to initialize MySQL: %s\n", mysql_error(mysql_conn));
		return 0;
	}

	if (!read_mysql_connect_file(mysql_connect_vars)) {
		fprintf(stderr, "Could not read database connection file.\n");
		return 0;
	}

	if (mysql_real_connect(mysql_conn, DB_CONNECT_STR(mysql_connect_vars), 0, NULL, 0) == NULL) {
		fprintf(stderr, "Failed to connect to MySQL: %s\n", mysql_error(mysql_conn));
		mysql_close(mysql_conn);
		mysql_conn = NULL;
		return 0;
	}

	return 1;
}

/*
Disconnects the MySQL connection.
*/
void db_close() {
	if (mysql_conn) {
		mysql_close(mysql_conn);
		// TODO
		// Only call this after the last connection is deleted.
		// e.g. at graceful shutdown.
		// Currently this makes the leak checker happy (valgrind).
		mysql_library_end();
	}
}

/*
Executes an SQL query.

Returns
	1 on success, 0 on failure.
*/
int db_execute_query(const char *query) {
	if (!mysql_conn) {
		fprintf(stderr, "MySQL not connected\n");
		return 0;
	}

	if (mysql_query(mysql_conn, query) != 0) {
		fprintf(stderr, "Query failed: %s\n", mysql_error(mysql_conn));
		return 0;
	}

	return 1;
}


/*
Retrieves ALL records and columns for the given table. e.g.
SELECT * from TABLENAME;
The returned value must be passed into a tbl_parse* function
so data is accessible by column name.

Example:
------------------------------------------------
st_tbl_trade_order_t *trade_order = parse_tbl_trade_order( db_fetch_data(...) );
if (trade_order) {
	trade_order->id
	trade_order->created_at
}

Params
	tbl_type: the enum representation of the table name

Returns
	A MYSQL_RES pointer to the retrieved data, this data needs to
	passed to a tbl_parse* function
*/
MYSQL_RES * db_fetch_data(DB_TBL_TYPE tbl_type) {
	MYSQL_RES *result;

	if (!mysql_conn) 
		return NULL;

	if (mysql_query(mysql_conn, database_query_str[tbl_type]) != 0) {
		fprintf(stderr, "Query failed: %s\n", mysql_error(mysql_conn));
		return NULL;
	}

	result = mysql_store_result(mysql_conn);
	if (!result) {
		fprintf(stderr, "Failed to store result: %s\n", mysql_error(mysql_conn));
		return NULL;
	}

	return result;
}

/*
This is the same as db_fetch_data except an additional SQL
conditional clause can be used. e.g.
WHERE name='bob' and id > 2 GROUP BY.. ORDER BY... ETC.

Params
	tbl_type: the enum representation of the table name
	sql: The additional SQL clause

Returns
	A MYSQL_RES pointer to the retrieved data, this data needs to
	passed to a tbl_parse* function
*/
MYSQL_RES * db_fetch_data_sql(DB_TBL_TYPE tbl_type, const char *sql) {
	void *data;
	char query_str[1024];
	MYSQL_RES *result;

	if (!sql) {
		fprintf(stderr, "Invalid SQL query (db_fetch_data_sql)\n");
		return NULL;
	}

	if (!mysql_conn) 
		return NULL;

	snprintf(query_str, 1024, "%s %s", database_query_str[tbl_type], sql);
	if (mysql_query(mysql_conn, query_str) != 0) {
		fprintf(stderr, "Query failed: %s\n", mysql_error(mysql_conn));
		return NULL;
	}

	result = mysql_store_result(mysql_conn);
	if (!result) {
		fprintf(stderr, "Failed to store result: %s\n", mysql_error(mysql_conn));
		return NULL;
	}

	return result;
}

/*
Creates a timestamp formatted like the TIMESTAMP database type.
The created timestamp will either be the current time, or the 
current time minus some number of weeks.

Params
	buffer: The timestamp is stored here.
	subtract_weeks: The number of weeks to subtract from the timestamp.
*/
void db_timestamp(char *buffer, unsigned short subtract_weeks) {
	time_t now;
	time(&now);
	// 7 * 24 * 60 * 60 = 604800
	now -= (subtract_weeks * 604800);
	strftime(buffer, TIMESTAMP_LEN, "%Y-%m-%d %H:%M:%S", localtime(&now));
}
