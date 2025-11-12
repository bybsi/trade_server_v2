#include "trade_service.h"
#include <signal.h>

static TradeService* service = NULL;

void handle_signal(int sig) {
    if (service) {
        trade_service_stop(service);
    }
    exit(0);
}

int main(void) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    service = trade_service_create();
    if (!service) {
        fprintf(stderr, "Failed to create trade service\n");
        return 1;
    }

    if (!trade_service_start(service)) {
        fprintf(stderr, "Failed to start trade service\n");
        trade_service_destroy(service);
        return 1;
    }

    while (1) {
        sleep(1);
    }

    return 0;
} 
