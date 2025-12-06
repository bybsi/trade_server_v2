#include <string.h>

#include "hiredis/hiredis.h"
#include "redis.h"

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379

/*
Executes a Redis 'get key' command.

Params
	redis: The redisContext.
	key: The key to retrieve data from.

Returns
	A malloc'd char * containing data found using get key.
	NULL on error or if no result is found.
*/
char * redis_get(redisContext *redis, char *key) {
	redisReply *reply;
	char *result = NULL;
	char cmd[128] = "get ";
	if (strlen(key) > 123)
		return NULL;
	strcat(cmd, key);

        reply = redisCommand(redis, cmd);
        if (reply->type == REDIS_REPLY_STRING && reply->str) {
		result = strdup(reply->str);
        } else if (reply->type == REDIS_REPLY_ERROR) {
                fprintf(stderr, "Error: redis_get\n");
        }

	freeReplyObject(reply);
	return result;
}

/*
Executes a redis command such as set KEY VALUE.

Params
	redis: The redisContext
	cmd: The command to execute.
*/
void redis_cmd(redisContext *redis, char *cmd) {
	freeReplyObject( redisCommand(redis, cmd) );
}

/*
Executes the redis lrange command.

Params
	redis: The redisContext.
	args: A string containing everything that goes after "lrange "

Returns
	Pointer to a redisReply struct.
	This pointer must be freed using freeReplyObject()
*/
redisReply * redis_lrange(redisContext *redis, char *args) {
	redisReply *reply;
	char cmd[128] = "lrange ";
	if (strlen(args) > 123)
		return NULL;
	strcat(cmd, args);

        reply = redisCommand(redis, cmd);
	if (reply->type != REDIS_REPLY_ARRAY) {
		fprintf(stderr, "Error: Unexpected redis return type.\n");
	} else if (reply->type == REDIS_REPLY_ERROR) {
		fprintf(stderr, "Error: redis_lrange\n");
	}
	return reply;
}

/*
Initializes the redis connection.

Returns
	A pointer to the redisContext.
	NULL on error.
*/
redisContext * redis_init() {
        redisContext *redis = redisConnect(REDIS_HOST, REDIS_PORT);

        if (!redis || redis->err) {
                if (redis) {
                        fprintf(stderr, "Error: %s\n", redis->errstr);
			redisFree(redis);
			redis = NULL;
		}
                else
                        fprintf(stderr, "Can't allocate redis context.\n");
        }

	return redis;
}

