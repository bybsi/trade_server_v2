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
} st_order_book;

// Main trade service structure
typedef struct st_trade_service {
    char *tickers[MAX_TICKERS];
    size_t ticker_count;
    
    st_order_book *order_books;
    FILE **price_sources;
    FILE **buy_sources;
    FILE **sell_sources;
    
    double *last_prices;
    pthread_t monitor_thread;
    int running;
    
    // Logging
    FILE *log_file;
    char *log_path;
    int log_level;
    
    // Service info
    char *name;
    pid_t pid;
    char *pid_file;
} st_trade_service;

st_trade_service *trade_service_create(void);
void trade_service_destroy(st_trade_service *service);
int trade_service_start(st_trade_service *service);
void trade_service_stop(st_trade_service *service);

int init_service(st_trade_service *service);
int init_logging(st_trade_service *service, const char *log_level);
int load_price_sources(st_trade_service *service);
int load_order_sources(st_trade_service *service);
int process_fills(st_trade_service *service, const char *ticker, double current_price);
void *market_monitor(void *arg);

void log_message(st_trade_service *service, const char *level, const char *format, ...);
double get_price_key(double price);

#endif // _TRADE_SERVICE_H_
