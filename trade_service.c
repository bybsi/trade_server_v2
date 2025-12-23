#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>
#include <ctype.h>

#include "hashtable.h"
#include "rb_tree.h"
#include "redis.h"
#include "database.h"
#include "trade_service.h"
#include "sse_server.h"
#include "error.h"

#define PORT 6262
#define DATA_Q_SIZE 10

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379

// Max number of orders to initially load from the database.
#define STR_MAX_ORDERS "5000"
// Closest prime to 25000 for hashtable distribution.
#define HT_ORDER_CAPACITY 25013

#define BACKLOAD_WEEKS 12

enum ticker {
	ANDTHEN = 0,
	FORIS4,
	SPARK,
	ZILBIAN,
	TICKER_COUNT
};
static const char *tickers[] = {
	"ANDTHEN",
	"FORIS4",
	"SPARK",
	"ZILBIAN",
	NULL
};
// Pointer to the logger instance for use inside of
// certain callback functions that don't have access to the
// service instance.
static ST_LOGGER *global_logger_ptr = NULL;

static unsigned short load_orders(ST_TRADE_SERVICE *service);
static unsigned short load_orders_all_tickers(ST_TRADE_SERVICE *service);
static unsigned short load_orders_helper(
	char side, char *order_by, unsigned short ticker_id, 
	HASHTABLE *ht_orders, RBT_NODE **rbt_orders, 
	char *last_order_read_time, char *id_prefix);
static void process_fills(ST_TRADE_SERVICE *service, 
	unsigned short ticker, unsigned long long current_price);
static unsigned short fill_order(ST_TBL_TRADE_ORDER *order);

static void strtolower(char *str) {
	for (unsigned short i = 0; *str; i++)
		str[i] = tolower(str[i]);
}

/*
Updates the redis key associated with a tickers current price.

This key is not used in SSE, however it can be used for other features.

Params
	redis: The redisContext.
	ticker_idx: The index of const char *tickers to use.
			generally defined as TICKER_<TICKERNAME>
			#define TICKER_ANDTHEN 1 (see above)
	pp: The price data to set the keys value to.
*/
static void update_redis_ticker_price(redisContext *redis, unsigned short ticker_idx, ST_PRICE_POINT *pp) {

	char cmd[128];
	snprintf(cmd, 128, "set %s-price_v2 %.6f:%d", tickers[ticker_idx], (double)(pp->price/100), pp->flag);
	redis_cmd(redis, cmd);
}

/*
Opens a file handle to each tickers price data.

Price data files are binary where each price is packed to 5 bytes
[float][byte]
4 bytes: price
1 byte:  candle flag (OPEN|CLOSE|LOW|HIGH) <-- TODO: needs a rework

Params
	service: The ST_TRADE_SERVICE instance.

Returns
	1 on success, 0 on failure.
*/
static int load_price_sources(ST_TRADE_SERVICE *service) {
	for (size_t i = 0; i < TICKER_COUNT; i++) {
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

/*
Reads the next price from a tickers price source file and places
the data into an ST_PRICE_POINT struct.
If an error occurs, the ST_PRICE_POINT struct will have a price of 0.

A call to read_price_source indicates that the price has changed
Params
	fh: The file descriptor to read from.
	price_point: The ST_PRICE_POINT struct to put the data into.
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

/*
TODO ??
Finishes initializing the trade service after
trade_service_start has been called.

Params
	service: The ST_TRADE_SERVICE instance.

Returns
	1 on success, 0 on failure
*/
static int init_service(ST_TRADE_SERVICE *service) {
	ST_PRICE_POINT pp = {0, 0};

	if (!db_init())
		return 0;

	service->redis = redis_init();
	if (!service->redis)
		return 0;

	if (!load_price_sources(service)) 
		return 0;
	
	// TODO - This price should come from redis current price.
	for (int ticker_idx = 0; ticker_idx < TICKER_COUNT; ticker_idx++) {
		read_price_source(service->price_sources[ticker_idx], &pp);
		if (pp.price == 0) {
			logger_write(service->logger, 
				"Could not load last price for TICKER: %s", 
				tickers[ticker_idx]);
			continue;
		}
		service->last_prices[ticker_idx].price = pp.price;
	}

	db_timestamp(service->last_order_read_time, BACKLOAD_WEEKS);
	if (!load_orders(service))
		return 0;

	return 1;
}

/* Appends to the hashtable key to make distribution more even
   especially if buy and sell orders are somehow reworked
   to potentially have conflicting IDs */
#define BUY_ORDER_ID_PREFIX "b"
#define SELL_ORDER_ID_PREFIX "s"

/*
Callback function which is used to map a hashtable node
to its associated DL_LIST *node.
This gives the order, access to the surrounding data structure if
it is accessed using a hashtable lookup. For example,
when an order is cancelled this will be useful.

Params
	ht_node: The hashtable entry stored in the orders hashtable.
	dll_node: The doubly linked list node stored as the data of
		  a red and black tree.
*/
void ht_node_to_dll_node(void *ht_node, void *dll_node) {
	((HT_ENTRY *)ht_node)->ref = dll_node;
}

// TODO #defines
//print_tbl_trade_order((ST_TBL_TRADE_ORDER *) ((HT_ENTRY *)data)->value);
//printf("next link: \n");
//printf("\t");
//DLL_NODE *next = ((DLL_NODE *)((HT_ENTRY *)data)->ref)->next;
//if (next)
//	print_tbl_trade_order(
//		((ST_TBL_TRADE_ORDER *)((HT_ENTRY *)next->data)->value)
//	);

/*
Loads the orders from the database for all trade tickers.

Params
	service: The ST_TRADE_SERVICE instance.

Returns
	1 on success, 0 on failure
	
*/
static unsigned short load_orders_all_tickers(ST_TRADE_SERVICE *service) {
	unsigned short ticker_idx;
	unsigned short result;
	for (ticker_idx = 0; ticker_idx < TICKER_COUNT; ticker_idx++) {
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

/*
Loads orders from the database defined by the given query params.

Params
	side: 'B' for buy or 'S' for sell
	order_by: "ASC" or "DESC"
	ticker_id: The index of const char *tickers to use.
			generally defined as TICKER_<TICKERNAME>
			#define TICKER_ANDTHEN 1 (see above)
	ht_orders: The hashtable containing orders.
	rbt_orders: The red and black tree containing orders.
	last_order_read_time: The timestamp of the time this function ran, 
				used to only grab newly added orders.
	id_prefix: see #define BUY_ORDER_ID_PREFIX

Returns
	1 on success, 0 on error.
*/
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
			// TODO maybe a DLL_NODE * should be returned 
			// to set the ht_orders->ref field instead of
			// using a callback.
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

/*
Loads buy and sell orders into memory and populates datastructures.
Updates the last_order_read_time value.

Params
	service: The ST_TRADE_SERVICE instance.

Returns
	1 on success, 0 on failure.
*/
static unsigned short load_orders(ST_TRADE_SERVICE *service) {
//	unsigned short result = (load_buy_orders(service) & load_sell_orders(service));
	unsigned short result = load_orders_all_tickers(service);
	db_timestamp(service->last_order_read_time, 0);
	return result;
}

/*
Takes an order and fills it, meaning that it is marked as
filled and money / assets have been transferred the appropriate places.

Params
	order: A pointer to the order to fill.
*/
static unsigned short fill_order(ST_TBL_TRADE_ORDER *order) {
	char sql[1024];
	char ticker[TICKER_LEN];
	return 0;
	// TODO run these in the same transaction as both need to succeed.
	db_timestamp(order->filled_at, 0);
	// TODO Need an order status of Pending or Failed/Retry
	order->status = 'F';

	snprintf(sql, 1024, "UPDATE %s SET status='%c' filled_at='%s' WHERE id=%lu",
		database_tbl_names[TBL_TRADE_ORDER], 
		order->status, order->filled_at, order->id);
	if (!db_execute_query(sql)) {
		fprintf(stderr, "Unable to fill order: %lu for user: %u\n", 
			order->id, order->user_id);
		logger_write(global_logger_ptr, "Unable to fill order: %lu", order->id);
		return 0;
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
		logger_write(global_logger_ptr, "Unable to fill order: %lu", order->id);
		return 0;
	}

	order->status = 'F';
	return 1;
}

/*
This is a callback function that gets executed as part of a
red and black tree traversal / search / find.

Params
	data: This will be a doubly linked list and is the data of
		the red and black tree node.
*/
void order_visitor(void *data) {
	DL_LIST *order_list;
	DLL_NODE *current_node, *tmp_node;
	HT_ENTRY *ht_entry;
	ST_TBL_TRADE_ORDER *order;

	order_list = (DL_LIST *) data;
	current_node = order_list->head->next;
	// TODO need a better way to iterate the dl_list and delete
	// while on the current node.
	while (current_node) {
		order = (ST_TBL_TRADE_ORDER *)((HT_ENTRY *) current_node->data)->value;

		printf("Filling order: %ld\n", order->id);
		if (fill_order(order)) {
			// TODO still need to delete item from the hashtable,
			// need a global pointer, it is a global hashtable afterall..
			// but it's stored in ST_TRADE_SERVICE... rework maybe??
			tmp_node = current_node->next;
			printf("Removing order\n");
			dl_list_remove(order_list, current_node);
			current_node = tmp_node;
		} else {
			current_node = current_node->next;
		}
	}
}

/*
Performs a search based on the current and last ticker price for open orders.
Those orders are then processed using a visitor callback function (above).

Params
	service: The ST_TRADE_SERVICE instance.
	ticker: The index of const char *tickers to use.
		generally defined as TICKER_<TICKERNAME>
		#define TICKER_ANDTHEN 1 (see above)
	current_price: The current market price of the ticker
*/
static void process_fills(ST_TRADE_SERVICE *service, unsigned short ticker, unsigned long long current_price) {
	unsigned long long low_price, high_price;

	if (service->last_prices[ticker].price == 0)
		// This will occur if there was an issue loading
		// the initial last price in init_service() 'innit'
		return;

	if (current_price > service->last_prices[ticker].price) {
		high_price = current_price;
		low_price = service->last_prices[ticker].price;
	} else {
		high_price = service->last_prices[ticker].price;
		low_price = current_price;
	}
	printf("Checking orders between: %lld and %lld\n", low_price, high_price);
	// TODO these can each run in a new thread! 
	// TODO means we need more than one SQL connection!
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

/*
Initializes the trade service instance.

Returns
	An ST_TRADE_SERVICE pointer or NULL on error.
*/
ST_TRADE_SERVICE *trade_service_init(void) {
	unsigned short i;

	ST_TRADE_SERVICE *service = calloc(1, sizeof(ST_TRADE_SERVICE));
	if (!service) {
		ERROR_MEMORY("service");
	}
	service->datapoint_count = 0;
	service->ht_orders = ht_init(HT_ORDER_CAPACITY, NULL);
	// Allocate arrays based on ticker count
	service->order_books = calloc(TICKER_COUNT, sizeof(ST_ORDER_BOOK));
	if (!service->order_books) {
		ERROR_MEMORY("order books");
	}
	service->last_prices = calloc(TICKER_COUNT, sizeof(ST_PRICE_POINT));
	if (!service->last_prices) {
		ERROR_MEMORY("last prices");
	}
	for (i = 0; i < TICKER_COUNT; i++) {
		// Initialize data structures to handle orders
		service->order_books[i].rbt_buy_orders = rbt_init();
		service->order_books[i].rbt_sell_orders = rbt_init();
		service->last_prices[i].price = 0;
		service->last_prices[i].flag = 0;
	}
	service->price_sources = calloc(TICKER_COUNT, sizeof(FILE*));
	if (!service->price_sources) {
		ERROR_MEMORY("price sources");
	}
	service->logger = logger_init("trade_service.log");
	if (!service->logger)
		return NULL;
	global_logger_ptr = service->logger;
	
	if (!service->order_books || 
		!service->price_sources || 
		!service->last_prices) {
		trade_service_destroy(service);
		return NULL;
	}

	return service;
}

/*
Deletes the trade service instance.

Params
	service: The ST_TRADE_SERVICE instance to delete.
*/
void trade_service_destroy(ST_TRADE_SERVICE *service) {
	if (!service) 
		return;

	for (size_t ticker_idx = 0; ticker_idx < TICKER_COUNT; ticker_idx++) {
		
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

/*
Starts the trade service in a new thread.

Params
	service: The ST_TRADE_SERVICE instance.

Returns
	1 on success, 0 on failure.
*/
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

/*
Stops the trade service.

Params
	service: The ST_TRADE_SERICE instance.
*/
void trade_service_stop(ST_TRADE_SERVICE *service) {
	if (!service) 
		return;

	service->running = 0;
	pthread_join(service->monitor_thread, NULL);
	logger_write(service->logger, "Trade service stopped");
	//trade_service_destroy()
}

/*
The market_monitor thread loop which is used by trade_service_start.

Params
	arg: This will be an ST_TRADE_SERVICE struct.
*/
void *market_monitor(void *arg) {
	
	unsigned short ticker_idx;
	ST_TRADE_SERVICE *service = (ST_TRADE_SERVICE*)arg;
	ST_PRICE_POINT current_price = {0, 0};
	while (service->running) {
		for (ticker_idx = 0; ticker_idx < TICKER_COUNT; ticker_idx++) {
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

