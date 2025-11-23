#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hiredis/hiredis.h"

#define ADDR "127.0.0.1"
#define PORT 6379

int main(int argc, char *argv[]) {
	unsigned short i;
	redisContext *c = redisConnect(ADDR, PORT);

	if (!c || c->err) {
		if (c)
			printf("Error: %s\n", c->errstr);
		else
			printf("Can't allocate redis context.");
		exit(1);
	}

	redisReply *reply = redisCommand(c, "lrange ANDTHEN-x -1 -1");
	if (reply->type == REDIS_REPLY_ARRAY) {
		for (i = 0; i < reply->elements; i++) {
			printf("Array String Type: %s\n", reply->element[i]->str);
		}
	} else if (reply->type == REDIS_REPLY_STRING) {
		printf("String Type: %s\n", reply->str);
	} else if (reply->type == REDIS_REPLY_ERROR) {
		printf("Error\n");
	}
	freeReplyObject(reply);

	redisFree(c);
	exit(0);
} 
