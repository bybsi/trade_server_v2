#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>

#include "hashtable.h"
#include "rb_tree.h"
#include "hiredis/hiredis.h"
#include "database.h"
#include "trade_service.h"
#include "sse_server.h"

#define PORT 6262
#define DATA_Q_SIZE 10

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379

// Max number of orders to initially load from the database.
#define STR_MAX_ORDERS "5000"
// Closest prime to 25000 for hashtable distribution.
#define HT_ORDER_CAPACITY 25013

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
static redisContext * redis_init();

static unsigned short load_orders(ST_TRADE_SERVICE *service);
static unsigned short load_orders_all_tickers(ST_TRADE_SERVICE *service);
static unsigned short load_orders_helper(
	char side, char *order_by, unsigned short ticker_id, 
	HASHTABLE *ht_orders, RBT_NODE **rbt_orders, 
	char *last_order_read_time, char *id_prefix);
static double get_price_key(double price);
static int process_fills(ST_TRADE_SERVICE *service, const char *ticker, double current_price);

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

	db_timestamp(service->last_order_read_time, BACKLOAD_WEEKS);
	if (!load_orders(service))
		return 0;

	return 1;
}

#define BUY_ORDER_ID_PREFIX "b"
#define SELL_ORDER_ID_PREFIX "s"
void ht_node_to_dll_node(void *ht_node, void *dll_node) {
	((HT_ENTRY *)ht_node)->ref = dll_node;
}

// TODO define macros possibly 
//print_tbl_trade_order((ST_TBL_TRADE_ORDER *) ((HT_ENTRY *)data)->value);
//printf("next link: \n");
//printf("\t");
//DLL_NODE *next = ((DLL_NODE *)((HT_ENTRY *)data)->ref)->next;
//if (next)
//	print_tbl_trade_order(
//		((ST_TBL_TRADE_ORDER *)((HT_ENTRY *)next->data)->value)
//	);

static unsigned short load_orders_all_tickers(ST_TRADE_SERVICE *service) {
	unsigned short ticker_idx;
	unsigned short result;
	for (ticker_idx = 0; ticker_idx < service->ticker_count; ticker_idx++) {
		result = load_orders_helper(
				'B', "ASC", ticker_idx, service->ht_orders,
				&service->order_books[ticker_idx].rbt_buy_orders,
				service->last_order_read_time, BUY_ORDER_ID_PREFIX);
		if (!result)
			//TODO cleanup
			return 0;
		result = load_orders_helper(
				'S', "DESC", ticker_idx, service->ht_orders,
				&service->order_books[ticker_idx].rbt_sell_orders,
				service->last_order_read_time, SELL_ORDER_ID_PREFIX);
		if (!result)
			//TODO cleanup
			return 0;
	}
	return 1;
}

static unsigned short load_orders_helper(
	char side, char *order_by, unsigned short ticker_id, 
	HASHTABLE *ht_orders, RBT_NODE **rbt_orders, 
	char *last_order_read_time, char *id_prefix) {
	
	HT_ENTRY *ht_order_entry;
	ST_TBL_TRADE_ORDER *result_head;
	ST_TBL_TRADE_ORDER *current_result, *tmp_result;
	static char sql[1024], hash_key[16];
	snprintf(sql, 1024, 
"WHERE side='%c' and status='O' and ticker='%s' and created_at >= '%s' "
"ORDER BY price %s, created_at ASC "
"LIMIT " STR_MAX_ORDERS, 
		side,
		tickers[ticker_id],
		last_order_read_time,
		order_by);
	result_head = (ST_TBL_TRADE_ORDER *) db_fetch_data_sql(TBL_TRADE_ORDER, sql);
	// head is a dummy node ... should refactor this so there is no dummy
	current_result = result_head->next;
	while (current_result) {
		tmp_result = current_result;
		// TODO this could cause double orders if hashtable put
		// returns an existing element due to identical keys.
		// In that case we need to leave the hashtable put as is,
		// because it updates the order, however the rbt->dll list
		// shouldn't be updated because adding a new list link
		// effectively puts two list links mapping to the same
		// order, and that could be bad... should be fine since
		// we delete an order after it's gone from the hashtable...
		// but it still leaves a broken reference in one of the list nodes.
		// and could later cause a crash.
		snprintf(hash_key, 16, "%s%lu", id_prefix, current_result->id);
		ht_order_entry = ht_put(ht_orders, hash_key, (void *) current_result);
		if (ht_order_entry) {
			rbt_insert(
				rbt_orders,
				current_result->price,
				ht_order_entry,
				&ht_node_to_dll_node
			);
			current_result = tmp_result->next;
		} else {
			// Free the current node after moving to the next
			// because it is not referenced to in the order book.
			current_result = tmp_result->next;
			free(tmp_result);
		}
	}
	// Free dummy node
	free(result_head);
	// TODO error handling
	return 1;
}

static unsigned short load_orders(ST_TRADE_SERVICE *service) {
//	unsigned short result = (load_buy_orders(service) & load_sell_orders(service));
	unsigned short result = load_orders_all_tickers(service);
	db_timestamp(service->last_order_read_time, 0);
	return result;
}

static double get_price_key(double price) {
	return 0.0;
} 

static int process_fills(ST_TRADE_SERVICE *service, const char *ticker, double current_price) {
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
	for (i = 0; i < service->ticker_count; i++) {
		// Initialize datastructures to handle orders
		service->order_books[i].rbt_buy_orders = rbt_init();
		service->order_books[i].rbt_sell_orders = rbt_init();
		service->ht_orders = ht_init(HT_ORDER_CAPACITY, NULL);
	}
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

	for (size_t ticker_idx = 0; ticker_idx < service->ticker_count; ticker_idx++) {
		
		if (service->price_sources && service->price_sources[ticker_idx])
			fclose(service->price_sources[ticker_idx]);
		
		if (service->order_books) {
			ST_ORDER_BOOK *book = &service->order_books[ticker_idx];
			// TODO book destroy
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

