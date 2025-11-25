CC = gcc -O3
CCDBG = gcc -g3 -O0
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS = 

DBTABLES := $(wildcard tbl_*.c)
WITH_MYSQL := $(shell mysql_config --cflags) $(shell mysql_config --libs)
WITH_REDIS := -I/usr/local/include/hiredis -lhiredis

dbtest: database.c dbtest.c $(DBTABLES)
	$(CCDBG) $(LDFLAGS) database.c dbtest.c $(DBTABLES) -o dbtest $(WITH_MYSQL)

cwtest: sse_client_writer.c sse_client_writer_test.c logger.c
	$(CCDBG) $(LDFLAGS) sse_client_writer.c sse_client_writer_test.c logger.c -o cwtest

servertest: sse_client_writer.c logger.c sse_server.c sse_server_test.c
	$(CCDBG) $(LDFLAGS) sse_client_writer.c logger.c sse_server.c sse_server_test.c -o servertest

httest: hashtable.c hashtable_test.c
	$(CCDBG) $(LDFLAGS) hashtable.c hashtable_test.c -o httest

redistest: redis_test.c
	$(CCDBG) $(LDFLAGS) redis_test.c $(WITH_REDIS) -o redistest

rbtest: dl_list.c rb_tree.c rb_tree_test.c database.c $(DBTABLES)
	$(CCDBG) $(LDFLAGS) dl_list.c rb_tree.c rb_tree_test.c database.c $(DBTABLES) -o rbtest $(WITH_MYSQL)

servicetest: hashtable.c dl_list.c rb_tree.c trade_service_test.c database.c $(DBTABLES)
	$(CCDBG) $(LDFLAGS) hashtable.c dl_list.c rb_tree.c trade_service_test.c database.c $(DBTABLES) -o servicetest $(WITH_MYSQL)

tradeservice: database.c dl_list.c hashtable.c logger.c \
		rb_tree.c sse_client_writer.c sse_server.c \
		trade_service.c $(DBTABLES)
	$(CCDBG) $(LDFLAGS) database.c dl_list.c hashtable.c \
	logger.c rb_tree.c sse_client_writer.c sse_server.c \
	trade_service.c $(DBTABLES) \
	-o tradeservice \
	$(WITH_MYSQL) \
	$(WITH_REDIS)

clean:
	rm -f dbtest cwtest servertest httest redistest rbtest servicetest tradeservice
