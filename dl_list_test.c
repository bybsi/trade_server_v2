#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dl_list.h"

static void print_list(dl_list_t *dl) {
	dll_node_t *node;
	node = dl->head;
	printf("-----------------\n");
	while (node = node->next) {
		printf("Node: %s\n", (char *)node->data);
	}
}

int main() {
	dll_node_t *node;
	char *data0 = strdup("HELLO 0");
	char *data1 = strdup("HELLO 1");
	char *data2 = strdup("HELLO 2");
	char *data3 = strdup("HELLO 3");
	dl_list_t *dl = dl_list_init(NULL);
	dl_list_insert(dl, (void*)data0, NULL);
	dl_list_insert(dl, (void*)data1, NULL);
	dl_list_insert(dl, (void*)data2, NULL);
	dl_list_insert(dl, (void*)data3, NULL);

	print_list(dl);

	dl_list_remove(dl, dl->head->next);
	dl_list_remove(dl, dl->head->next);
	dl_list_remove(dl, dl->head->next);
	dl_list_remove(dl, dl->head->next);
	dl_list_remove(dl, dl->head->next);

	print_list(dl);
	
	dl_list_insert(dl, (void*)data0, NULL);
	dl_list_insert(dl, (void*)data1, NULL);
	dl_list_insert(dl, (void*)data2, NULL);
	dl_list_insert(dl, (void*)data3, NULL);

	print_list(dl);
	dl_list_remove(dl, dl->head->next->next->next->next);
	print_list(dl);

	return 0;
}

