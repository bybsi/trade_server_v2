#ifndef _REDIS_H_
#define _REDIS_H_

#include <hiredis/hiredis.h>

char * redis_get(redisContext *redis, char *key);
void redis_cmd(redisContext *redis, char *cmd);
redisReply * redis_lrange(redisContext *redis, char *args);
redisContext * redis_init();

#endif // _REDIS_H_

