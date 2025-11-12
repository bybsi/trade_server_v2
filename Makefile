CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS = 

#all: myprogram

#myprogram: main.o helper.o
#	$(CC) $(LDFLAGS) main.o helper.o -o myprogram

#main.o: main.c
#	$(CC) $(CFLAGS) -c main.c -o main.o

#helper.o: helper.c
#	$(CC) $(CFLAGS) -c helper.c -o helper.o

#clean:
#	rm -f myprogram main.o helper.o
DBTABLES := $(wildcard tbl_*.c)
WITH_MYSQL := $(shell mysql_config --cflags) $(shell mysql_config --libs)

dbtest: database.c dbtest.c $(DBTABLES)
	$(CC) $(LDFLAGS) database.c dbtest.c $(DBTABLES) -o dbtest $(WITH_MYSQL)

clean:
	rm -f dbtest
