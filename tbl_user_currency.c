#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <time.h>
#include <mysql/mysql.h>
#include "tbl_user_currency.h"

void * parse_user_currency(MYSQL_RES *result) {
	char *end;
	MYSQL_ROW row;
	struct tbl_user_currency *head, *current; 
	unsigned long tmpul;
	
	errno = 0;
	head = malloc(sizeof(TBL_USER_CURRENCY));
	head->next = NULL;
	current = head;
	while ((row = mysql_fetch_row(result)) != NULL) {
		// Build new node
		struct tbl_user_currency *tto = malloc(sizeof(TBL_USER_CURRENCY));
		tto->user_id = strtoi(row[0], &end, 10);
		tto->bybs    = strtoull(row[1], &end, 10);
		tto->andthen = strtoull(row[2], &end, 10);
		tto->foris4  = strtoull(row[3], &end, 10);
		tto->zilbian = strtoull(row[4], &end, 10);
		tto->spark   = strtoull(row[5], &end, 10);
		tto->next = NULL;
		// End new node

		current->next = tto;
		current = tto;
	}
	return (*void) head;
}


