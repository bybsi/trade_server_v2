#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <time.h>
#include <mysql/mysql.h>
#include "tbl_user_currency.h"

ST_TBL_USER_CURRENCY * parse_tbl_user_currency(MYSQL_RES *result) {
	char *end;
	MYSQL_ROW row;
	ST_TBL_USER_CURRENCY *head, *current; 
	
	if (!result) {
		fprintf(stderr, "Parsing tbl_user_currency (NULL)\n");
		return NULL;
	}
	
	errno = 0;
	head = malloc(sizeof(ST_TBL_USER_CURRENCY));
	head->next = NULL;
	current = head;
	while ((row = mysql_fetch_row(result)) != NULL) {
		// Build new node
		struct tbl_user_currency *uc = malloc(sizeof(ST_TBL_USER_CURRENCY));
		// TODO library function for bounds checking conversion.
		uc->user_id = (unsigned int) strtoul(row[0], &end, 10);
		uc->bybs    = strtoull(row[1], &end, 10);
		uc->andthen = strtoull(row[2], &end, 10);
		uc->foris4  = strtoull(row[3], &end, 10);
		uc->zilbian = strtoull(row[4], &end, 10);
		uc->spark   = strtoull(row[5], &end, 10);
		uc->next = NULL;
		// End new node

		current->next = uc;
		current = uc;
	}
	
	mysql_free_result(result);
	return head;
}

void free_user_currency(ST_TBL_USER_CURRENCY *head) {
	ST_TBL_USER_CURRENCY *next;
	while (head) {
		next = head->next;
		free(head);
		head = next;
	}
}


