#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>
#include <ctype.h>

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
static void process_fills(ST_TRADE_SERVICE *service, 
	unsigned short ticker, unsigned long long current_price);

static void strtolower(char *str) {
	for (unsigned short i = 0; *str; i++)
		str[i] = tolower(str[i]);
}

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
		fprintf(stderr, "Error: Unexpected redis return type.\n");
	} else if (reply->type == REDIS_REPLY_ERROR) {
		fprintf(stderr, "Error: redis_lrange\n");
	}
	return reply;
}

static redisContext * redis_init() {
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

// This key is not used in SSE, however it can be used for other features.
static void update_redis_ticker_price(redisContext *redis, unsigned short ticker_idx, ST_PRICE_POINT *pp) {

	char cmd[128];
	snprintf(cmd, 128, "set %s-price_v2 %.6f:%d", tickers[ticker_idx], (double)(pp->price/100), pp->flag);
	redis_cmd(redis, cmd);
}

/**
* Opens a file handle to each tickers price data.
*
* Price data files are binary where each price is packed to 5 bytes
* [float][byte]
* 4 bytes: price
* 1 byte:  candle flag (OPEN|CLOSE|LOW|HIGH) <-- TODO: needs a rework
*/
static int load_price_sources(ST_TRADE_SERVICE *service) {
	for (size_t i = 0; i < service->ticker_count; i++) {
		char filepath[256];
		/* /var/data/filename */
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

/**
* Reads the next price from a tickers price source file and places
* the data into ST_PRICE_POINT *price_point.
* If there is an error the price_point will have a price of 0.
*/
static void read_price_source(FILE *fh, ST_PRICE_POINT *price_point) {
	float price;
	unsigned char flag_byte;
	
	price_point->price = 0;
	if (fread(&price, sizeof(float), 1, fh) != 1) {
		fprintf(stderr, "Couldn't read price from price source.\n");
		return;
	}
	price_point->price = (unsigned long long) (price * 100);
	price_point->flag = 0;
	if (fread(&flag_byte, 1, 1, fh) != 1) {
		price_point->price = 0;
		fprintf(stderr, "Couldn't read flag_byte from price source.\n");
		return;
	}
	price_point->flag = flag_byte;
}

/**
* Finishes initializing the trade service after
* trade_service_start has been called.
*/
static int init_service(ST_TRADE_SERVICE *service) {
	if (!db_init())
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
/**
* Callback function which is used to map a hashtable node
* to its associated DL_LIST *node.
* This gives the order, and us, access to the surrounding data structure if
* it is accessed using a hashtable lookup, for example if it's cancelled.
*/
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

// TODO add logging .... hmm
void fill_order(ST_TBL_TRADE_ORDER *order) {
	char sql[1024];
	char ticker[TICKER_LEN];
	return;
	// TODO run these in the same transaction as both need to succeed.
	db_timestamp(order->filled_at, 0);
	order->status = 'F';

	snprintf(sql, 1024, "UPDATE %s SET status='%c' filled_at='%s' WHERE id=%lu",
		database_tbl_names[TBL_TRADE_ORDER], 
		order->status, order->filled_at, order->id);
	if (!db_execute_query(sql)) {
		fprintf(stderr, "Unable to fill order: %lu for user: %u\n", 
			order->id, order->user_id);
		//logger_write(service->logger, "Unable to fill order: %lu", order->id);
		return;
	}

	snprintf(ticker, TICKER_LEN, "%s", order->ticker);
	strtolower(ticker);
	if (order->side == 'B')
		snprintf(sql, 1024, "UPDATE %s SET %s=%s + %u WHERE user_id=%u", 
			database_tbl_names[TBL_USER_CURRENCY], 
			ticker, ticker, order->amount, order->user_id);
	else
		snprintf(sql, 1024, "UPDATE %s SET bybs=bybs + (%llu * %u) WHERE user_id=%u",
			database_tbl_names[TBL_USER_CURRENCY],
			order->price, order->amount, order->user_id);
	if (!db_execute_query(sql)) {
		fprintf(stderr, 
			"Unable to update user wallet: order_id:%lu, user_id:%u\n", 
			order->id, order->user_id);
		//logger_write(service->logger, "Unable to fill order: %lu", order->id);
		return;
	}
}

// TODO add logging .... hmm
void order_visitor(void *data) {
	DL_LIST *order_list;
	DLL_NODE *current_node;
	HT_ENTRY *ht_entry;
	ST_TBL_TRADE_ORDER *order;

	order_list = (DL_LIST *) data;
	current_node = order_list->head->next;

	while (current_node) {
		order = (ST_TBL_TRADE_ORDER *)((HT_ENTRY *) current_node->data)->value;

		printf("Filling order: %ld\n", order->id);
		// TODO delete from HT and DL_LIST
		fill_order(order);

		current_node = current_node->next;
	}
}

// TODO fill in last_prices[ticker] before execuing this loop so
// we don't have to check if it's set.
static void process_fills(ST_TRADE_SERVICE *service, unsigned short ticker, unsigned long long current_price) {
	unsigned long long low_price, high_price;
	if (current_price > service->last_prices[ticker].price) {
		high_price = current_price;
		low_price = service->last_prices[ticker].price;
	} else {
		high_price = service->last_prices[ticker].price;
		low_price = current_price;
	}
	printf("Checking orders between: %lld and %lld\n", low_price, high_price);
	rbt_visit_nodes_in_range(
		service->order_books[ticker].rbt_buy_orders,
		low_price, // low node key
		high_price, // high  node key
		&order_visitor);
	rbt_visit_nodes_in_range(
		service->order_books[ticker].rbt_sell_orders,
		low_price,
		high_price,
		&order_visitor);
}


ST_TRADE_SERVICE *trade_service_init(void) {
	unsigned short i;

	ST_TRADE_SERVICE *service = calloc(1, sizeof(ST_TRADE_SERVICE));
	if (!service) 
		return NULL; // TODO memory error DEFINE

	service->ticker_count = sizeof(tickers) / sizeof(tickers[0]) - 1;

	service->ht_orders = ht_init(HT_ORDER_CAPACITY, NULL);
	// Allocate arrays based on ticker count
	service->order_books = calloc(service->ticker_count, sizeof(ST_ORDER_BOOK));
	if (!service->order_books)
		return NULL; // TODO memory error DEFINE
	
	service->last_prices = calloc(service->ticker_count, sizeof(ST_PRICE_POINT));
	if (!service->last_prices)
		return NULL; // TODO memory error DEFINE
	
	for (i = 0; i < service->ticker_count; i++) {
		// Initialize data structures to handle orders
		service->order_books[i].rbt_buy_orders = rbt_init();
		service->order_books[i].rbt_sell_orders = rbt_init();

		service->last_prices[i].price = 0;
		service->last_prices[i].flag = 0;
	}
	service->price_sources = calloc(service->ticker_count, sizeof(FILE*));
	if (!service->price_sources)
		return NULL; // TODO memory error DEFINE
	
	service->logger = logger_init("trade_service.log");
	if (!service->logger)
		return NULL;
	
	service->datapoint_count = 0;

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

	if (service->logger) {
		logger_close(service->logger);
		free(service->logger);
	}
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
	//trade_service_destroy()
}

void *market_monitor(void *arg) {
	
	unsigned short ticker_idx;
	ST_TRADE_SERVICE *service = (ST_TRADE_SERVICE*)arg;
	ST_PRICE_POINT current_price;
	while (service->running) {
		for (ticker_idx = 0; ticker_idx < service->ticker_count; ticker_idx++) {
			current_price.price = 0;
			current_price.flag = 0;
			read_price_source(service->price_sources[ticker_idx], &current_price);
			if (!current_price.price) {
				fprintf(stderr, 
					"Couldn't read price data source. Ticker idx: %s\n", 
					tickers[ticker_idx]);
				continue;
			}
			update_redis_ticker_price(service->redis, ticker_idx, &current_price);
			process_fills(service, ticker_idx, current_price.price);
			service->last_prices[ticker_idx].price = current_price.price;
			service->last_prices[ticker_idx].flag = current_price.flag;
		}
		sleep(2); 
	}
	return NULL;
}

