#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>

#include "hiredis/hiredis.h"
#include "database.h"
#include "trade_service.h"
#include "sse_server.h"

#define PORT 6262
#define DATA_Q_SIZE 10

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379

#define STR_MAX_ORDERS "25000"
#define BACKLOAD_WEEKS 12

#define TICKER_ANDTHEN 0
#define TICKER_FORIS4 1
#define TICKER_SPARK 2
#define TICKER_ZILBIAN 3
static const char *tickers[] = {
	"ANDTHEN",
	"FORIS4",
	"SPARK",
	"ZILBIAN",
	NULL
};

static const size_t DEFAULT_TICKER_COUNT = 4;

// TODO: move redis into own module just like the database
static char * redis_get(redisContext *redis, char *key);
static void redis_cmd(redisContext *redis, char *cmd);
static redisReply * redis_lrange(redisContext *redis, char *args);
static *redisContext redis_init();

static char * redis_get(redisContext *redis, char *key) {
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

static void redis_cmd(redisContext *redis, char *cmd) {
	freeReplyObject( redisCommand(redis, cmd) );
}

// Caller must use freeReplyObject()
static redisReply * redis_lrange(redisContext *redis, char *args) {
	redisReply *reply;
	char cmd[128] = "lrange ";
	if (strlen(args) > 123)
		return NULL;
	strcat(cmd, args);

        reply = redisCommand(redis, cmd);
	if (reply->type != REDIS_REPLY_ARRAY) {
		fprintf(stderr, "Error: unexpected redis return type\n");
	} else if (reply->type == REDIS_REPLY_ERROR) {
		fprintf(stderr, "Error: redis_lrange\n");
	}
	return reply;
}

static redisContext * redis_init() {
        redisContext *redis = redisConnect(REDIS_HOST, REDIS_PORT);

        if (!redis || redis->err) {
                if (redis) {
                        printf("Error: %s\n", redis->errstr);
			redisFree(redis);
			redis = NULL;
		}
                else
                        printf("Can't allocate redis context.");
        }

	return redis;
}

static int init_service(ST_TRADE_SERVICE *service) {
	if (!db_init())
		return 0;

	service->logger = logger_init("trade_service.log");
	if (!service->logger)
		return 0;

	service->redis = redis_init();
	if (!service->redis)
		return 0;

	if (!load_price_sources(service)) 
		return 0;

	service->last_order_read_time = db_timestamp(BACKLOAD_WEEKS);
	if (!load_orders(service))
		return 0;

	return 1;
}

static int load_price_sources(ST_TRADE_SERVICE *service) {
	for (size_t i = 0; i < service->ticker_count; i++) {
		char filepath[256];
		snprintf(filepath, sizeof(filepath), "%s/%s0.points.10s", 
				DATA_DIR, tickers[i]);
		
		service->price_sources[i] = fopen(filepath, "rb");
		if (!service->price_sources[i]) {
			logger_write(service->logger, "Failed to open price file: %s", filepath);
			return 0;
		}
	}
	return 1;
}

static unsigned short load_buy_orders(ST_TRADE_SERVICE *service) {
	ST_TBL_TRADE_ORDER *tbl_trade_order;
	static char sql[1024];
	snprintf(sql, 1024, 
"WHERE side='B' and status='O' and created_at >= '%s' "
"ORDER BY price ASC, created_at ASC "
"LIMIT " STR_MAX_ORDERS, 
		service->last_order_read_time);
	tbl_trade_order = (ST_TBL_TRADE_ORDER *) db_fetch_data_sql(TBL_TRADE_ORDER, sql);
	
}

static unsigned short load_orders(ST_TRADE_SERVICE *service) {
	unsigned short result = (load_buy_orders(service) & load_sell_orders(service));
	service->last_order_read_time = db_timestamp(0);
	return result;
}

static double get_price_key(double price) {
	// TODO price should be unsigned long long
	return round(price * 100000000.0) / 100000000.0; 
} 

static int process_fills(ST_TRADE_SERVICE *service, const char *ticker, double current_price) {
	size_t ticker_idx;
	for (ticker_idx = 0; ticker_idx < service->ticker_count; ticker_idx++) {
		if (strcmp(tickers[ticker_idx], ticker) == 0)
			break;
	}
	if (ticker_idx == service->ticker_count) 
		return 0;

	ST_ORDER_BOOK *book = &service->order_books[ticker_idx];
	double prev_price = service->last_prices[ticker_idx];

	// Process buy orders
	for (size_t i = 0; i < book->buy_count; i++) {
		st_price_point *pp = &book->buy_points[i];
		if (pp->price >= prev_price && pp->price <= current_price) {
			// Process fills at this price point
			for (size_t j = 0; j < pp->order_count; j++) {
				// TODO: Implement actual order filling logic
				logger_write(service->logger, "Fill buy order for %s at %f", 
						  ticker, pp->price);
			}
		}
	}

	// Process sell orders
	for (size_t i = 0; i < book->sell_count; i++) {
		st_price_point *pp = &book->sell_points[i];
		if (pp->price <= prev_price && pp->price >= current_price) {
			// Process fills at this price point
			for (size_t j = 0; j < pp->order_count; j++) {
				// TODO: Implement actual order filling logic
				logger_write(service->logger, "Fill sell order for %s at %f", 
						  ticker, pp->price);
			}
		}
	}

	return 1;
}


ST_TRADE_SERVICE *trade_service_create(void) {
	unsigned short i;

	ST_TRADE_SERVICE *service = calloc(1, sizeof(ST_TRADE_SERVICE));
	if (!service) 
		return NULL;

	service->ticker_count = sizeof(tickers) / sizeof(tickers[0]) - 1;
	fprintf(stderr, "%d", service->ticker_count);

	// Allocate arrays based on ticker count
	service->order_books = calloc(service->ticker_count, sizeof(ST_ORDER_BOOK));
	service->price_sources = calloc(service->ticker_count, sizeof(FILE*));
	service->last_prices = calloc(service->ticker_count, sizeof(double));

	if (!service->order_books || 
		!service->price_sources || 
		!service->last_prices) {
		trade_service_destroy(service);
		return NULL;
	}

	return service;
}

void trade_service_destroy(ST_TRADE_SERVICE *service) {
	if (!service) 
		return;

	for (size_t i = 0; i < service->ticker_count; i++) {
		
		if (service->price_sources && service->price_sources[i])
			fclose(service->price_sources[i]);
	// NEED AVL tree.
		if (service->order_books) {
			ST_ORDER_BOOK *book = &service->order_books[i];
			for (size_t j = 0; j < book->buy_count; j++)
				free(book->buy_points[j].orders);
			for (size_t j = 0; j < book->sell_count; j++)
				free(book->sell_points[j].orders);
			free(book->buy_points);
			free(book->sell_points);
		}
	}

	free(service->order_books);
	free(service->price_sources);
	free(service->last_prices);

	logger_close(service->logger);
	if (service->redis)
		redisFree(service->redis);
	free(service);
}

int trade_service_start(ST_TRADE_SERVICE *service) {
	if (!service) 
		return 0;

	logger_write(service->logger, "Starting trade service");

	if (!init_service(service)) {
		logger_write(service->logger, "Failed to initialize service");
		trade_service_destroy(service);
		return 0;
	}

	service->running = 1;
	if (pthread_create(&service->monitor_thread, NULL, market_monitor, service) != 0) {
		logger_write(service->logger, "Failed to create monitor thread");
		service->running = 0;
		return 0;
	}

	return 1;
}

void trade_service_stop(ST_TRADE_SERVICE *service) {
	if (!service) 
		return;

	service->running = 0;
	pthread_join(service->monitor_thread, NULL);
	logger_write(service->logger, "Trade service stopped");
}

void *market_monitor(void *arg) {
	ST_TRADE_SERVICE *service = (ST_TRADE_SERVICE*)arg;
	
	while (service->running) {
		for (size_t i = 0; i < service->ticker_count; i++) {
			FILE *price_file = service->price_sources[i];
			if (!price_file) 
				continue;

			// Read price data (8 bytes for double)
			// ...
			double price;
			if (fread(&price, sizeof(double), 1, price_file) == 1) {
				process_fills(service, tickers[i], price);
				service->last_prices[i] = price;
			}
		}
		sleep(2); 
	}
	return NULL;
}

