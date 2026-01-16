CCREL = gcc -O3
CCDBG = gcc -g3 -O0
CFLAGS = -Wall -Wextra
LDFLAGS = 
CC = $(CCDBG)
#CC = $(CCREL)

DBTABLES := $(wildcard tbl_*.c)
WITH_MYSQL := $(shell mysql_config --cflags) $(shell mysql_config --libs)
WITH_REDIS := -I/usr/local/include/hiredis -lhiredis
WITH_DS := -lds
LIBDS := data_structures/libds.so
DSLINK := -Wl,-rpath='.' -L./data_structures -I./data_structures

dbtest: database.c dbtest.c $(DBTABLES)
	$(CC) $(LDFLAGS) database.c dbtest.c $(DBTABLES) -o dbtest $(WITH_MYSQL)

cwtest: sse_client_writer.c sse_client_writer_test.c logger.c
	$(CC) $(LDFLAGS) sse_client_writer.c sse_client_writer_test.c logger.c -o cwtest

servertest: sse_client_writer.c logger.c sse_server.c sse_server_test.c
	$(CC) $(LDFLAGS) sse_client_writer.c logger.c sse_server.c sse_server_test.c -o servertest

redistest: redis_test.c
	$(CC) $(LDFLAGS) redis_test.c $(WITH_REDIS) -o redistest

servicetest: database.c logger.c currency.c \
		redis.c sse_client_writer.c sse_server.c \
		trade_service.c trade_service_test.c $(DBTABLES) $(LIBDS)
	$(CC) $(LDFLAGS) $(DSLINK) database.c logger.c currency.c \
	redis.c sse_client_writer.c sse_server.c \
	trade_service.c trade_service_test.c $(DBTABLES) \
	-o servicetest \
	$(WITH_MYSQL) \
	$(WITH_REDIS) \
	$(WITH_DS)

tradeservice: database.c logger.c currency.c \
		redis.c sse_client_writer.c sse_server.c \
		trade_service.c $(DBTABLES) $(LIBDS)
	$(CC) $(LDFLAGS) database.c logger.c currency.c \
	redis.c sse_client_writer.c sse_server.c \
	trade_service.c $(DBTABLES) \
	-o tradeservice \
	$(WITH_MYSQL) \
	$(WITH_REDIS) \
	$(WITH_DS)

test: dltest httest rbtest redistest servertest servicetest

rbtest: rb_tree_test.c database.c $(DBTABLES) $(LIBDS)
	$(CC) $(LDFLAGS) $(DSLINK) rb_tree_test.c database.c $(DBTABLES) -o rbtest $(WITH_MYSQL) $(WITH_DS)
	cp $(LIBDS) .

dltest: dl_list_test.c $(LIBDS)
	$(CC) $(LDFLAGS) $(DSLINK) dl_list_test.c -o dltest $(WITH_DS)

httest: hashtable_test.c $(LIBDS)
	$(CC) $(LDFLAGS) $(DSLINK) hashtable_test.c -o httest $(WITH_DS)

clean:
	rm -f dbtest dltest cwtest servertest httest redistest rbtest servicetest tradeservice
