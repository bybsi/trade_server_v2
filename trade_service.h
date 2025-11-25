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

#include "hiredis/hiredis.h"
#include "logger.h"

#define MAX_TICKERS 10
#define MAX_ORDERS 25000
#define TICKER_LENGTH 10
#define DATA_DIR "/var/data/trading"
#define BUFFER_SIZE 1024
#define BUY_IDX 0
#define SELL_IDX 1

// Order structure (13 bytes when packed)
typedef struct st_order {
	char ticker[TICKER_LENGTH];
	double price;
	int user_id;
} __attribute__((packed)) st_order;

// Price point structure to store orders at a specific price
typedef struct st_price_point {
	double price;
	st_order *orders;
	size_t order_count;
	size_t capacity;
} st_price_point;

// Order book structure for a single ticker
typedef struct st_order_book {
	st_price_point *buy_points;
	st_price_point *sell_points;
	size_t buy_count;
	size_t sell_count;
	size_t buy_capacity;
	size_t sell_capacity;
} ST_ORDER_BOOK;

// Main trade service structure
typedef struct st_trade_service {
	char *tickers[MAX_TICKERS];
	size_t ticker_count;
	
	st_order_book *order_books;
	FILE **price_sources;
	
	double *last_prices;
	pthread_t monitor_thread;

	int running;
	// Logging
	ST_LOGGER *sse_logger;

	time_t last_order_read_time;

	redisContext *redis;
} ST_TRADE_SERVICE;

ST_TRADE_SERVICE *trade_service_init(void);
void trade_service_destroy(st_trade_service *service);
int trade_service_start(st_trade_service *service);
void trade_service_stop(st_trade_service *service);

void *market_monitor(void *arg);

#endif // _TRADE_SERVICE_H_
