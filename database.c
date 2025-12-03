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

// Single connection for now.
MYSQL *mysql_conn = NULL;

/*
Reads the .dbconnect file having format:
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

/* Parse functions are defined in each tbl_*.c file.
   The order of this array must match the DB_TBL_TYPE enum order. */
typedef void * (*tbl_mapper)(MYSQL_RES *);
tbl_mapper tbl_map[] = { 
	parse_tbl_trade_order,
	parse_tbl_user_currency
};

/*
Retrieves ALL records and columns for the given table. e.g.
SELECT * from TABLENAME;
While the return type is a void *, it will be a struct that's tied
to the table definition and will have be accessible by column name,
after being cast. e.g.:
	data->id
	data->created_at
	...

Params
	tbl_type: the enum representation of the table name

Returns
	A void pointer to the retrieved data, this data needs to
	be cast to its associated table struct.
	If there is an error then it returns NULL.
*/
void* db_fetch_data(DB_TBL_TYPE tbl_type) {
	void *data;
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

	// Convert the raw SQL character data into its associated struct.
	data = tbl_map[tbl_type](result);

	mysql_free_result(result);
	return data;
}

/*
This is the same as db_fetch_data except an additional SQL
conditional clause can be used. e.g.
WHERE blah='blah' and id > 5 GROUP BY.. ORDER BY... ETC.

Params
	tbl_type: the enum representation of the table name
	sql: The additional SQL clause

Returns
	A void pointer to the retrieved data, this data needs to
	be cast to its associated table struct.
	If there is an error then it returns NULL.
*/
void* db_fetch_data_sql(DB_TBL_TYPE tbl_type, const char *sql) {
	void *data;
	char query_str[1024];
	MYSQL_RES *result;

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

	// Convert the raw SQL character data into its associated struct.
	data = tbl_map[tbl_type](result);

	mysql_free_result(result);
	return data;
}

/*
Creates a timestamp recognized by the TIMESTAMP database field type.
The timestamp will either be the current time, or the current time
X number of weeks in the past.

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
