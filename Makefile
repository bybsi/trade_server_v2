CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS = 

DBTABLES := $(wildcard tbl_*.c)
WITH_MYSQL := $(shell mysql_config --cflags) $(shell mysql_config --libs)

dbtest: database.c dbtest.c $(DBTABLES)
	$(CC) $(LDFLAGS) database.c dbtest.c $(DBTABLES) -o dbtest $(WITH_MYSQL)

clean:
	rm -f dbtest
