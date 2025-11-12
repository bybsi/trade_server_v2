#include "trade_service.h"
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *DEFAULT_TICKERS[] = {
	"ANDTHEN",
	"FORIS4",
	"SPARK",
	"ZILBIAN"
};

static const size_t DEFAULT_TICKER_COUNT = 4;

st_trade_service *trade_service_create(void) {
	st_trade_service *service = calloc(1, sizeof(st_trade_service));
	if (!service) 
		return NULL;

	// Initialize tickers
	service->ticker_count = DEFAULT_TICKER_COUNT;
	for (size_t i = 0; i < DEFAULT_TICKER_COUNT; i++) {
		service->tickers[i] = strdup(DEFAULT_TICKERS[i]);
		if (!service->tickers[i]) {
			trade_service_destroy(service);
			return NULL;
		}
	}

	// Allocate arrays based on ticker count
	service->order_books = calloc(service->ticker_count, sizeof(st_order_book));
	service->price_sources = calloc(service->ticker_count, sizeof(FILE*));
	service->buy_sources = calloc(service->ticker_count, sizeof(FILE*));
	service->sell_sources = calloc(service->ticker_count, sizeof(FILE*));
	service->last_prices = calloc(service->ticker_count, sizeof(double));

	if (!service->order_books || !service->price_sources || 
		!service->buy_sources || !service->sell_sources || !service->last_prices) {
		trade_service_destroy(service);
		return NULL;
	}

	// Set default values
	service->name = strdup("Trade Service");
	service->pid_file = strdup("/var/run/trade_service.pid");
	service->log_path = strdup("logs/trade.log");
	service->running = 0;

	if (!service->name || !service->pid_file || !service->log_path) {
		trade_service_destroy(service);
		return NULL;
	}

	return service;
}

void trade_service_destroy(st_trade_service *service) {
	if (!service) 
		return;

	for (size_t i = 0; i < service->ticker_count; i++) {
		free(service->tickers[i]);
		
		if (service->price_sources && service->price_sources[i])
			fclose(service->price_sources[i]);
		if (service->buy_sources && service->buy_sources[i])
			fclose(service->buy_sources[i]);
		if (service->sell_sources && service->sell_sources[i])
			fclose(service->sell_sources[i]);

		if (service->order_books) {
			st_order_book *book = &service->order_books[i];
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
	free(service->buy_sources);
	free(service->sell_sources);
	free(service->last_prices);

	if (service->log_file)
		fclose(service->log_file);

	free(service->name);
	free(service->pid_file);
	free(service->log_path);
	free(service);
}

int trade_service_start(st_trade_service *service) {
	if (!service) 
		return 0;

	log_message(service, "INFO", "Starting %s", service->name);

	if (!init_service(service)) {
		log_message(service, "ERROR", "Failed to initialize service");
		return 0;
	}

	service->running = 1;
	if (pthread_create(&service->monitor_thread, NULL, market_monitor, service) != 0) {
		log_message(service, "ERROR", "Failed to create monitor thread");
		service->running = 0;
		return 0;
	}

	service->pid = getpid();
	FILE *pid_file = fopen(service->pid_file, "w");
	if (pid_file) {
		fprintf(pid_file, "%d\n", service->pid);
		fclose(pid_file);
	}

	return 1;
}

void trade_service_stop(st_trade_service *service) {
	if (!service) 
		return;

	service->running = 0;
	pthread_join(service->monitor_thread, NULL);
	unlink(service->pid_file);
	log_message(service, "INFO", "%s stopped", service->name);
}

int init_service(st_trade_service *service) {
	if (!init_logging(service, "DEBUG")) 
		return 0;
	if (!load_price_sources(service)) 
		return 0;
	if (!load_order_sources(service)) 
		return 0;
	return 1;
}

int init_logging(st_trade_service *service, const char *log_level) {
	// Create logs directory if it doesn't exist
	mkdir("logs", 0755);

	service->log_file = fopen(service->log_path, "a");
	if (!service->log_file) {
		fprintf(stderr, "Failed to open log file: %s\n", strerror(errno));
		return 0;
	}

	setvbuf(service->log_file, NULL, _IOLBF, 0);
	return 1;
}

void *market_monitor(void *arg) {
	st_trade_service *service = (st_trade_service*)arg;
	
	while (service->running) {
		for (size_t i = 0; i < service->ticker_count; i++) {
			FILE *price_file = service->price_sources[i];
			if (!price_file) 
				continue;

			// Read price data (8 bytes for double)
			// ...
			double price;
			if (fread(&price, sizeof(double), 1, price_file) == 1) {
				process_fills(service, service->tickers[i], price);
				service->last_prices[i] = price;
			}
		}
		sleep(2); 
	}
	return NULL;
}

int process_fills(st_trade_service *service, const char *ticker, double current_price) {
	size_t ticker_idx;
	for (ticker_idx = 0; ticker_idx < service->ticker_count; ticker_idx++) {
		if (strcmp(service->tickers[ticker_idx], ticker) == 0)
			break;
	}
	if (ticker_idx == service->ticker_count) 
		return 0;

	st_order_book *book = &service->order_books[ticker_idx];
	double prev_price = service->last_prices[ticker_idx];

	// Process buy orders
	for (size_t i = 0; i < book->buy_count; i++) {
		st_price_point *pp = &book->buy_points[i];
		if (pp->price >= prev_price && pp->price <= current_price) {
			// Process fills at this price point
			for (size_t j = 0; j < pp->order_count; j++) {
				// TODO: Implement actual order filling logic
				log_message(service, "INFO", "Fill buy order for %s at %f", 
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
				log_message(service, "INFO", "Fill sell order for %s at %f", 
						  ticker, pp->price);
			}
		}
	}

	return 1;
}

int load_price_sources(st_trade_service *service) {
	for (size_t i = 0; i < service->ticker_count; i++) {
		char filepath[256];
		snprintf(filepath, sizeof(filepath), "%s/%s_prices.dat", 
				DATA_DIR, service->tickers[i]);
		
		service->price_sources[i] = fopen(filepath, "rb");
		if (!service->price_sources[i]) {
			log_message(service, "ERROR", "Failed to open price file: %s", filepath);
			return 0;
		}
	}
	return 1;
}

int load_order_sources(st_trade_service *service) {
	for (size_t i = 0; i < service->ticker_count; i++) {
		char buy_path[256], sell_path[256];
		snprintf(buy_path, sizeof(buy_path), "%s/%s_buy_orders.dat",
				DATA_DIR, service->tickers[i]);
		snprintf(sell_path, sizeof(sell_path), "%s/%s_sell_orders.dat",
				DATA_DIR, service->tickers[i]);
		
		service->buy_sources[i] = fopen(buy_path, "rb");
		service->sell_sources[i] = fopen(sell_path, "rb");
		
		if (!service->buy_sources[i] || !service->sell_sources[i]) {
			log_message(service, "ERROR", "Failed to open order files for %s",
					   service->tickers[i]);
			return 0;
		}
	}
	return 1;
}

void log_message(st_trade_service *service, const char *level, const char *format, ...) {
	if (!service || !service->log_file) 
		return;

	pthread_mutex_lock(&log_mutex);
	
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);
	char timestamp[20];
	strftime(timestamp, sizeof(timestamp), "%Y%m%d:%H%M%S", tm_info);

	fprintf(service->log_file, "%s [%s] ", timestamp, level);
	
	va_list args;
	va_start(args, format);
	vfprintf(service->log_file, format, args);
	va_end(args);
	
	fprintf(service->log_file, "\n");
	pthread_mutex_unlock(&log_mutex);
}

double get_price_key(double price) {
	// TODO price should be unsigned long long
	return round(price * 100000000.0) / 100000000.0; 
} 
