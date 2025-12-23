#ifndef _TRADE_SERVICE_H_
#define _TRADE_SERVICE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <pthread.h>

#include "database.h"
#include "rb_tree.h"
#include "hashtable.h"
#include "hiredis/hiredis.h"
#include "logger.h"

#define MAX_TICKERS 10
#define MAX_ORDERS 25000
#define TICKER_LENGTH 10
#define DATA_DIR "/var/data/trading"
#define BUFFER_SIZE 1024
#define BUY_IDX 0
#define SELL_IDX 1

// Order book structure for a single ticker
typedef struct st_order_book {
	RBT_NODE *rbt_buy_orders;
	RBT_NODE *rbt_sell_orders;
} ST_ORDER_BOOK;

typedef struct st_price_point {
	unsigned long long price;
	int flag;
} ST_PRICE_POINT;

// Main trade service structure
typedef struct ST_TRADE_SERVICE {
	// Order books contain a buy and sell list which are
	// red and black trees using price points for keys.
	// Tree node data is a doubly linked list of orders
	// at a given price point, ordered by priority,
	// or the order they were placed.
	// RBT_NODE -> DLL_NODE
	ST_ORDER_BOOK *order_books;
	// Quick access to cancel and delete orders
	// Keys are the order id and data is a DLL_NODE
	// that contains the order as it's data.
	// RBT_NODE -> DLL_NODE -> ORDER
	HASHTABLE *ht_orders;

	FILE **price_sources;
	ST_PRICE_POINT *last_prices;

	pthread_t monitor_thread;

	int running;
	unsigned int datapoint_count;
	// Logging
	ST_LOGGER *logger;

	char last_order_read_time[TIMESTAMP_LEN];

	redisContext *redis;
} ST_TRADE_SERVICE;

ST_TRADE_SERVICE *trade_service_init(void);
void trade_service_destroy(ST_TRADE_SERVICE *service);
int trade_service_start(ST_TRADE_SERVICE *service);
void trade_service_stop(ST_TRADE_SERVICE *service);

void *market_monitor(void *arg);

#endif // _TRADE_SERVICE_H_
