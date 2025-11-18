CC = gcc -O3
CCDBG = gcc -g3 -O0
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS = 

DBTABLES := $(wildcard tbl_*.c)
WITH_MYSQL := $(shell mysql_config --cflags) $(shell mysql_config --libs)

dbtest: database.c dbtest.c $(DBTABLES)
	$(CCDBG) $(LDFLAGS) database.c dbtest.c $(DBTABLES) -o dbtest $(WITH_MYSQL)

cwtest: sse_client_writer.c sse_client_writer_test.c logger.c
	$(CCDBG) $(LDFLAGS) sse_client_writer.c sse_client_writer_test.c logger.c -o cwtest

servertest: sse_client_writer.c logger.c sse_server.c sse_server_test.c
	$(CCDBG) $(LDFLAGS) sse_client_writer.c logger.c sse_server.c sse_server_test.c -o servertest
clean:
	rm -f dbtest cwtest servertest
