#ifndef _TBL_USER_CURRENCY_H
#define _TBL_USER_CURRENCY_H

typedef struct tbl_user_currency {
	unsigned int user_id;
	unsigned long long bybs;
	unsigned long long andthen;
	unsigned long long foris4;
	unsigned long long zilbian;
	unsigned long long spark;
	struct tbl_user_currency *next;
} ST_TBL_USER_CURRENCY;

ST_TBL_USER_CURRENCY * parse_tbl_user_currency(MYSQL_RES *result);
void free_user_currency(ST_TBL_USER_CURRENCY *head);

#endif
