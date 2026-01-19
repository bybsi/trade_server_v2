#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>

#include "hashtable.h"
#include "rb_tree.h"
#include "redis.h"
#include "database.h"
#include "trade_service.h"
#include "sse_server.h"
#include "error.h"
#include "currency.h"

#define PORT 6262
#define DATA_Q_SIZE 10

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379

// Max number of orders to initially load from the database.
#define STR_MAX_ORDERS "5000"
// Closest prime to 25000 for hashtable distribution.
#define HT_ORDER_CAPACITY 25013
#define BACKLOAD_WEEKS 12

// Pointer to the logger instance for use inside of
// certain callback functions that don't have access to the
// service instance.
static st_logger_t *global_logger_ptr = NULL;
static const char *tickers[] = {
	"ANDTHEN",
	"FORIS4",
	"SPARK",
	"ZILBIAN",
	NULL
};
unsigned short get_ticker_idx(char *ticker_name) {
	unsigned short i;
	for (i = 0; i < NUM_TICKERS; i++)
		if (strcmp(tickers[i], ticker_name) == 0)
			return i;
	return -1;
}

int exit_flag = 0;
pthread_mutex_t exit_lock;
void thread_check_exit_flag(char *thread_name) {
	pthread_mutex_lock(&exit_lock);
	if (exit_flag) {
		pthread_mutex_unlock(&exit_lock);
		printf("%s received exit flag.\n", thread_name);
		pthread_exit(NULL);
	}
	pthread_mutex_unlock(&exit_lock);
}

/* 
For batching fill data and sending it to the event stream. 
Data is dispatched every FILL_DISPATCH_INTERVAL seconds or 
when FILL_CAPACITY is reached.   
*/
#define FILL_CAPACITY 10
#define FILL_DISPATCH_INTERVAL 2 // seconds
struct {
	char fills[NUM_TICKERS][2048];
	unsigned short fill_count;
} fill_data = { {"","","",""}, 0};
/*
	.fills = {{""},{""},{""},{""}}, 
	.fill_count = 0
};
*/
//pthread_t fill_data_thread;
//pthread_mutex_t fill_data_lock;
unsigned short update_fill_data(st_tbl_trade_order_t * order); 
time_t dispatch_fill_data(st_trade_service_t *service);
time_t last_fill_dispatch_time;
//void *fill_data_dispatcher(void *arg);
/* ------ */

static unsigned short load_orders(st_trade_service_t *service);
static unsigned short load_orders_all_tickers(st_trade_service_t *service);
static unsigned short load_orders_helper(
	char side, char *order_by, unsigned short ticker_id, 
	hashtable_t *ht_orders, rbt_node_t **rbt_orders, 
	char *last_order_read_time, char *id_prefix);
static void process_fills(st_trade_service_t *service, 
	unsigned short ticker, unsigned long long current_price);
static unsigned short fill_order(st_tbl_trade_order_t *order);
void build_simulated_order(unsigned long long price, char *data);
int r_between(int min, int max);


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
static void update_redis_ticker_price(redisContext *redis, unsigned short ticker_idx, char *price_str) {

	char cmd[128];
	snprintf(cmd, 128, "set %s-price_v2 %s", tickers[ticker_idx], price_str);
	redis_cmd(redis, cmd);
}

/*
Opens a file handle to each tickers price data.

Price data files are binary where each price is packed to 5 bytes
[float][byte]
4 bytes: price
1 byte:  candle flag (OPEN|CLOSE|LOW|HIGH) <-- TODO: needs a rework

Params
	service: The st_trade_service_t instance.

Returns
	1 on success, 0 on failure.
*/
static int load_price_sources(st_trade_service_t *service) {
	for (size_t i = 0; i < NUM_TICKERS; i++) {
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
the data into an st_price_point_t struct.
If an error occurs, the st_price_point_t struct will have a price of 0.

A call to read_price_source indicates that the price has changed
Params
	fh: The file descriptor to read from.
	price_point: The st_price_point_t struct to put the data into.
*/
static void read_price_source(FILE *fh, st_price_point_t *price_point) {
	float price;
	unsigned char flag_byte;
	
	price_point->price = 0;
	if (fread(&price, sizeof(float), 1, fh) != 1) {
		fprintf(stderr, "Couldn't read price from price source.\n");
		return;
	}
	price_point->price = float_to_currency(price);
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
	service: The st_trade_service_t instance.

Returns
	1 on success, 0 on failure
*/
static int init_service(st_trade_service_t *service) {
	st_price_point_t pp = {0, 0};

	if (!db_init())
		return 0;

	service->redis = redis_init();
	if (!service->redis)
		return 0;

	if (!load_price_sources(service)) 
		return 0;
	
	// TODO - This price should come from redis current price.
	for (int ticker_idx = 0; ticker_idx < NUM_TICKERS; ticker_idx++) {
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
to its associated dl_list_t *node.
This gives the order, access to the surrounding data structure if
it is accessed using a hashtable lookup. For example,
when an order is cancelled this will be useful.

Params
	ht_node: The hashtable entry stored in the orders hashtable.
	dll_node: The doubly linked list node stored as the data of
		  a red and black tree.
*/
void ht_node_to_dll_node(void *ht_node, void *dll_node) {
	((ht_entry_t *)ht_node)->ref = dll_node;
}

/*
Loads the orders from the database for all trade tickers.

Params
	service: The st_trade_service_t instance.

Returns
	1 on success, 0 on failure
	
*/
static unsigned short load_orders_all_tickers(st_trade_service_t *service) {
	unsigned short ticker_idx;
	unsigned short result;
	for (ticker_idx = 0; ticker_idx < NUM_TICKERS; ticker_idx++) {
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
	hashtable_t *ht_orders, rbt_node_t **rbt_orders, 
	char *last_order_read_time, char *id_prefix) {
	
	ht_entry_t *ht_order_entry;
	st_tbl_trade_order_t *result_head;
	st_tbl_trade_order_t *current_result, *tmp_result;
	static char sql[1024], hash_key[16];
	snprintf(sql, 1024, 
"WHERE side='%c' and status='O' and ticker='%s' and created_at >= '%s' "
"ORDER BY price %s, created_at ASC "
"LIMIT " STR_MAX_ORDERS, 
		side,
		tickers[ticker_id],
		last_order_read_time,
		order_by);

	printf("%s\n", sql);
	result_head = parse_tbl_trade_order( db_fetch_data_sql(TBL_TRADE_ORDER, sql) );
	if (!result_head) { 
		// TODO error handling
		fprintf(stderr, "Found no new orders\n");
		return 1;
	}
	
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
			// TODO maybe a dll_node_t * should be returned 
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
	service: The st_trade_service_t instance.

Returns
	1 on success, 0 on failure.
*/
static unsigned short load_orders(st_trade_service_t *service) {
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
static unsigned short fill_order(st_tbl_trade_order_t *order) {
	time_t now;
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
	
	/* TODO move into function) */
	if (!update_fill_data(order)) {
		unsigned short junk;
		//last_fill_dispatch_time = dispatch_fill_data(service);//TODO service
		junk = update_fill_data(order);
	} else if (time(NULL) > last_fill_dispatch_time + 2) {
		// Current time is greater than next dispatch time
		//last_fill_dispatch_time = dispatch_fill_data(service); // TODO service
	}

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
	dl_list_t *order_list;
	dll_node_t *current_node, *tmp_node;
	ht_entry_t *ht_entry;
	st_tbl_trade_order_t *order;

	order_list = (dl_list_t *) data;
	current_node = order_list->head->next;
	// TODO need a better way to iterate the dl_list and delete
	// while on the current node.
	while (current_node) {
		order = (st_tbl_trade_order_t *)((ht_entry_t *) current_node->data)->value;

		printf("Filling order: %ld\n", order->id);
		if (fill_order(order)) {
			// TODO still need to delete item from the hashtable,
			// need a global pointer, it is a global hashtable afterall..
			// but it's stored in st_trade_service_t... rework maybe??
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
	service: The st_trade_service_t instance.
	ticker: The index of const char *tickers to use.
		generally defined as TICKER_<TICKERNAME>
		#define TICKER_ANDTHEN 1 (see above)
	current_price: The current market price of the ticker
*/
static void process_fills(st_trade_service_t *service, unsigned short ticker, unsigned long long current_price) {
	unsigned long long low_price, high_price;

	pthread_mutex_lock(&service->last_price_lock);
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
	pthread_mutex_unlock(&service->last_price_lock);

	printf("Checking orders between: %lld and %lld\n", low_price, high_price);
	// TODO these can each run in a new thread! 
	// TODO means we need more than one SQL connection!
	// TODO maybe use a state saving mechanism to traverse
	//  instead of using a callback.
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
	An st_trade_service_t pointer or NULL on error.
*/
st_trade_service_t *trade_service_init(st_sse_server_t *server) {
	unsigned short i;
	
	st_trade_service_t *service = calloc(1, sizeof(st_trade_service_t));
	if (!service) {
		EXIT_OOM("service");
	}
	
	service->logger = logger_init("trade_service.log");
	if (!service->logger)
		return NULL;
	global_logger_ptr = service->logger;
	
	pthread_mutex_init(&service->last_price_lock, NULL);
	pthread_mutex_init(&exit_lock, NULL);
	//pthread_mutex_init(&fill_data_lock, NULL);

	service->datapoint_count = 0;
	service->ht_orders = ht_init(HT_ORDER_CAPACITY, NULL);
	for (i = 0; i < NUM_TICKERS; i++) {
		// Initialize data structures to handle orders
		service->order_books[i].rbt_buy_orders = rbt_init();
		service->order_books[i].rbt_sell_orders = rbt_init();
		service->last_prices[i].price = 0;
		service->last_prices[i].flag = 0;
	}
	
	service->server = server;
	last_fill_dispatch_time = time(NULL);

	return service;
}

/*
Deletes the trade service instance.

Params
	service: The st_trade_service_t instance to delete.
*/
void trade_service_destroy(st_trade_service_t *service) {
	if (!service) 
		return;

	for (size_t ticker_idx = 0; ticker_idx < NUM_TICKERS; ticker_idx++) {
		
		if (service->price_sources && service->price_sources[ticker_idx])
			fclose(service->price_sources[ticker_idx]);
		
		if (service->order_books) {
			st_order_book_t *book = &service->order_books[ticker_idx];
			// TODO book destroy
		}
	}

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
	service: The st_trade_service_t instance.

Returns
	1 on success, 0 on failure.
*/
int trade_service_start(st_trade_service_t *service) {
	if (!service) 
		return 0;

	logger_write(service->logger, "Starting trade service");

	if (!init_service(service)) {
		logger_write(service->logger, "Failed to initialize service");
		trade_service_destroy(service);
		return 0;
	}

	if (pthread_create(&service->monitor_thread, NULL, market_monitor, service) != 0) {
		logger_write(service->logger, "Failed to create monitor thread");
		return 0;
	}
	
	if (pthread_create(&service->sim_order_thread, NULL, simulated_order_worker, service) != 0) {
		logger_write(service->logger, "Failed to create monitor thread");
		pthread_join(service->monitor_thread, NULL);
		return 0;
	}
	
	/*
	if (pthread_create(&fill_data_thread, NULL, fill_data_dispatcher, service) != 0) {
		logger_write(service->logger, "Failed to create fill data dispatcher thread");
		pthread_join(service->monitor_thread, NULL);
		pthread_join(service->sim_order_thread, NULL);
		return 0;
	}
	*/
	
	return 1;
}

/*
Stops the trade service.

Params
	service: The st_trade_service_t instance.
*/
void trade_service_stop(st_trade_service_t *service) {
	if (!service) 
		return;

	pthread_mutex_lock(&exit_lock);
	exit_flag = 1;
	pthread_mutex_unlock(&exit_lock);
	pthread_join(service->monitor_thread, NULL);
	pthread_join(service->sim_order_thread, NULL);
	logger_write(service->logger, "Trade service stopped");
	//trade_service_destroy()
}

// /event: BI\ndata:\n(xxxxxxxxxxxx.aaaaaa:0,){4}/
//(6*NUM_TICKERS) + (4*NUM_TICKERS) + (12*NUM_TICKERS) + 1
#define TMP_PRICE_STR_LEN 22
//#define SSE_PRICE_DATA_LEN (TMP_PRICE_STR_LEN * NUM_TICKERS) + 1
#define SSE_PRICE_DATA_LEN 89

/*
The market_monitor thread loop which is initiated by calling
trade_service_start. It is responsible for:
	1) Keeping the current asset prices up to date.
	2) Reading new orders into the data structures.
	3) Filling orders.
	4) Sending data to the client writer queue for front-end updates.
Params
	arg: This will be an st_trade_service_t struct.
*/
void *market_monitor(void *arg) {
	
	unsigned short ticker_idx;
	st_trade_service_t *service = (st_trade_service_t*)arg;
	st_price_point_t current_price = {0, 0};
	char sse_price_data[SSE_PRICE_DATA_LEN];
	char tmp_str[TMP_PRICE_STR_LEN];

	srand(time(NULL));

	while (1) {
		thread_check_exit_flag("Market monitor");
		
		sse_price_data[0] = '\0';
		strcat(sse_price_data, "event: Y\ndata: ");
		for (ticker_idx = 0; ticker_idx < NUM_TICKERS; ticker_idx++) {
			read_price_source(service->price_sources[ticker_idx], &current_price);
			if (!current_price.price) {
				fprintf(stderr, 
					"Couldn't read price data source. Ticker idx: %s\n", 
					tickers[ticker_idx]);
				continue;
			}

			process_fills(service, ticker_idx, current_price.price);
			/* price data TODO move into function*/
			pthread_mutex_lock(&service->last_price_lock);
			service->last_prices[ticker_idx].price = current_price.price;
			service->last_prices[ticker_idx].flag = current_price.flag;
			pthread_mutex_unlock(&service->last_price_lock);
			currency_to_string_extra(tmp_str, TMP_PRICE_STR_LEN, current_price.price, current_price.flag);
			update_redis_ticker_price(service->redis, ticker_idx, tmp_str);
			strcat(sse_price_data, tmp_str);
			if (ticker_idx != NUM_TICKERS - 1)
				strcat(sse_price_data, ",");
		}

		strcat(sse_price_data, "\n\n");
		sse_server_queue_data(service->server, sse_price_data);	
		sleep(2);
		load_orders(service);
	}
	return NULL;
}

#define SIM_ORDER_LEN 2048
void *simulated_order_worker(void *arg) {
	unsigned short ticker_idx;
	char sim_order_data[SIM_ORDER_LEN];
	st_trade_service_t *service = (st_trade_service_t*)arg;
	
	while (1) {
		thread_check_exit_flag("Simulated order worker");
		
		sim_order_data[0] = '\0';
		strcat(sim_order_data, "event: BU\ndata: ");
		for (ticker_idx = 0; ticker_idx < NUM_TICKERS; ticker_idx++) {
			/* simulated order data */
			strcat(sim_order_data, "\"");
			strcat(sim_order_data, tickers[ticker_idx]);
			strcat(sim_order_data, "\":[");
			pthread_mutex_lock(&service->last_price_lock);
			build_simulated_order(
				service->last_prices[ticker_idx].price, sim_order_data);
			pthread_mutex_unlock(&service->last_price_lock);
			strcat(sim_order_data, "]");
			if (ticker_idx != NUM_TICKERS - 1)
				strcat(sim_order_data, ",");
		}
		strcat(sim_order_data, "}\n\n");
		sse_server_queue_data(service->server, sim_order_data);	
		sleep(3);
	}
}

#define NUM_ORDER_MULTIPLIERS 20
float order_multipliers[NUM_ORDER_MULTIPLIERS] = {
	0.02, 0.005, 0.004, 0.003, 0.002,
	0.01, 0.02, 0.015, 0.011, 0.012,
	0.013, 0.001, 0.002, 0.003, 0.004,
	0.015, 0.011, 0.1, 0.05, 0.025
};

void build_simulated_order(unsigned long long price, char *data) {
	int i, amount;
	int rand_int = r_between(1, 5);
	char buffer[64];
	for (i = 0; i < rand_int; i++) {
		snprintf(buffer, 64, "[%d,%.6f,%d,%.6f]",
			r_between(2, 17),
			currency_to_float(price - fractional_price(price, order_multipliers[r_between(0, NUM_ORDER_MULTIPLIERS - 1)])),
			r_between(3, 15), 
			currency_to_float(price + fractional_price(price, order_multipliers[r_between(0, NUM_ORDER_MULTIPLIERS - 1)])));
		strcat(data, buffer);
		if (i != rand_int - 1)
			strcat(data, ",");
	}
}

unsigned short update_fill_data(st_tbl_trade_order_t * order) {
	char fill_str[128];
	unsigned short ticker_idx = get_ticker_idx(order->ticker);
	snprintf(fill_str, 128, "[%u,\"%c\",%u,%llu,%s],",
		order->user_id,
		order->side,
		order->amount,
		order->price,
		order->filled_at + 11);
	if (strlen(fill_str) + strlen(fill_data.fills[ticker_idx]) >= 2048)
		return 0;
	
	strcat(fill_data.fills[ticker_idx], fill_str);
	fill_data.fill_count++;
	return 1;
}

char fill_data_buffer[2048*4 + 20*NUM_TICKERS];
time_t dispatch_fill_data(st_trade_service_t *service) {
	unsigned short i, len;
	memcpy(fill_data_buffer, (char[]){'{', '\0'}, 2);
	for (i = 0; i < NUM_TICKERS; i++) {
		len = strlen(fill_data.fills[i]);
		if (len)
			// Remove the last comma
			fill_data.fills[i][len - 1] = '\0';
		strcat(fill_data_buffer, "\"");
		strcat(fill_data_buffer, tickers[i]);
		strcat(fill_data_buffer, "\":[");
		strcat(fill_data_buffer, fill_data.fills[i]);
		strcat(fill_data_buffer, "]");
		if (i != NUM_TICKERS - 1)
			strcat(fill_data_buffer, ",");
		fill_data.fills[i][0] = '\0';
	}
	strcat(fill_data_buffer, "}");
	sse_server_queue_data(service->server, fill_data_buffer);
	fill_data.fill_count = 0;
}

/*
void *fill_data_dispatcher(void *arg) {
	st_trade_service_t *service = (st_trade_service_t*)arg;
	
	while (1) {
		thread_check_exit_flag("Simulated order worker");
		sleep(FILL_DISPATCH_INTERVAL);
	}
}
*/
//rand_int = (rand() % 10) + 1; //1 and 10 inclusive

int r_between(int min, int max) {
	return (rand() % (max - min + 1)) + min;
}















