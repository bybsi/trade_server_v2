CCREL = gcc -O3
CCDBG = gcc -g3 -O0
CFLAGS = -Wall -Wextra -fPIC
LDFLAGS = 
CC = $(CCDBG)
#CC = $(CCREL)


rb.o: rb_tree.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@ 

dl.o: dl_list.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@

ht.o: hashtable.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@

libds.so: ht.o dl.o rb.o
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o libds.so ht.o dl.o rb.o

all: libds.so

clean:
	rm -f *.o *.so

