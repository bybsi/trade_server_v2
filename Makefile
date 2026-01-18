CCREL = gcc -O3
CCDBG = gcc -g3 -O0
CFLAGS = -Wall -Wextra
LDFLAGS = 
CC = $(CCDBG)
#CC = $(CCREL)

DB := database/database.c $(wildcard database/tbl_*.c)
DBI := -I./database
#DBTABLES := $(wildcard database/tbl_*.c)
WITH_MYSQL := $(shell mysql_config --cflags) $(shell mysql_config --libs)
WITH_REDIS := -I/usr/local/include/hiredis -lhiredis
WITH_DS := -lds
LIBDS := data_structures/libds.so
DSLINK := -Wl,-rpath='.' -L./data_structures -I./data_structures

dbtest: $(DB) dbtest.c
	$(CC) $(LDFLAGS) $(DBI) $(DB) dbtest.c  -o dbtest $(WITH_MYSQL)

cwtest: sse_client_writer.c test/sse_client_writer_test.c logger.c
	$(CC) $(LDFLAGS) sse_client_writer.c test/sse_client_writer_test.c logger.c -o cwtest

servertest: sse_client_writer.c logger.c sse_server.c test/sse_server_test.c
	$(CC) $(LDFLAGS) sse_client_writer.c logger.c sse_server.c test/sse_server_test.c -o servertest

redistest: test/redis_test.c
	$(CC) $(LDFLAGS) test/redis_test.c $(WITH_REDIS) -o redistest

servicetest: $(DB) logger.c currency.c \
		redis.c sse_client_writer.c sse_server.c \
		trade_service.c test/trade_service_test.c $(LIBDS)
	$(CC) $(LDFLAGS) $(DSLINK) $(DBI) $(DB) logger.c currency.c \
	redis.c sse_client_writer.c sse_server.c \
	trade_service.c test/trade_service_test.c  \
	-o servicetest \
	$(WITH_MYSQL) \
	$(WITH_REDIS) \
	$(WITH_DS) -I.

tradeservice: $(DB) logger.c currency.c \
		redis.c sse_client_writer.c sse_server.c \
		trade_service.c $(LIBDS)
	$(CC) $(LDFLAGS) $(DBI) $(DB) logger.c currency.c \
	redis.c sse_client_writer.c sse_server.c \
	trade_service.c \
	-o tradeservice \
	$(WITH_MYSQL) \
	$(WITH_REDIS) \
	$(WITH_DS)

test: dltest httest rbtest redistest servertest servicetest

rbtest: test/rb_tree_test.c $(DB) $(LIBDS)
	$(CC) $(LDFLAGS) $(DSLINK) test/rb_tree_test.c $(DBI) $(DB) -o rbtest $(WITH_MYSQL) $(WITH_DS)
	cp $(LIBDS) .

dltest: test/dl_list_test.c $(LIBDS)
	$(CC) $(LDFLAGS) $(DSLINK) test/dl_list_test.c -o dltest $(WITH_DS)

httest: test/hashtable_test.c $(LIBDS)
	$(CC) $(LDFLAGS) $(DSLINK) test/hashtable_test.c -o httest $(WITH_DS)

clean:
	rm -f dbtest dltest cwtest servertest httest redistest rbtest servicetest tradeservice

