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

#include "sse_server.h"
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

enum ticker {
	ANDTHEN = 0,
	FORIS4,
	SPARK,
	ZILBIAN,
	NUM_TICKERS
};

// Order book structure for a single ticker
typedef struct st_order_book {
	// TODO free these in destroy function
	rbt_node_t *rbt_buy_orders;
	rbt_node_t *rbt_sell_orders;
} st_order_book_t;

typedef struct st_price_point {
	unsigned long long price;
	int flag;
} st_price_point_t;

// Main trade service structure
typedef struct st_trade_service_t {
	// Order books contain a buy and sell list which are
	// red and black trees using price points for keys.
	// Tree node data is a doubly linked list of orders
	// at a given price point, ordered by priority,
	// or the order they were placed.
	// rbt_node_t -> dll_node_t
	//st_order_book_t *order_books;
	st_order_book_t order_books[NUM_TICKERS];
	// Quick access to cancel and delete orders
	// Keys are the order id and data is a dll_node_t
	// that contains the order as it's data.
	// rbt_node_t -> dll_node_t -> ORDER
	hashtable_t *ht_orders;

	FILE *price_sources[NUM_TICKERS];
	st_price_point_t last_prices[NUM_TICKERS];
	pthread_mutex_t last_price_lock;

	pthread_t monitor_thread;
	pthread_t sim_order_thread;

	int running;
	unsigned int datapoint_count;
	// Logging
	st_logger_t *logger;

	char last_order_read_time[TIMESTAMP_LEN];

	redisContext *redis;
	st_sse_server_t *server;
} st_trade_service_t;

st_trade_service_t *trade_service_init(st_sse_server_t *server);
void trade_service_destroy(st_trade_service_t *service);
int trade_service_start(st_trade_service_t *service);
void trade_service_stop(st_trade_service_t *service);

/* Thread workers */
void *market_monitor(void *arg);
void *simulated_order_worker(void *arg);

#endif // _TRADE_SERVICE_H_
