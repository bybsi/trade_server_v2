#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "currency.h"

// scaling factor (10^6)
#define SCALE_FACTOR 1000000LL 
#define DECIMAL_PLACES 6

currency_t float_to_currency(float amount) {
	return (currency_t)round(amount * SCALE_FACTOR);
}

double currency_to_float(currency_t amount) {
	return (float)amount / SCALE_FACTOR;
}

void currency_to_string(char *buffer, unsigned short buffer_size, currency_t amount) {
	currency_t whole = amount / SCALE_FACTOR;
	currency_t fractional = amount % SCALE_FACTOR;
	//if (fractional < 0) {
	 //   fractional = -fractional;
	//}
	snprintf(buffer, buffer_size, "%lld.%06lld", whole, fractional);
}

void currency_to_string_extra(char *buffer, unsigned short buffer_size, currency_t amount, unsigned short flag) {
	currency_t whole = amount / SCALE_FACTOR;
	currency_t fractional = amount % SCALE_FACTOR;
	//if (fractional < 0) {
	 //   fractional = -fractional;
	//}
	snprintf(buffer, buffer_size, "%lld.%06lld:%d", whole, fractional, flag);
	
}

currency_t fractional_price(currency_t price, float multiplier) {
	return (currency_t) round(price / float_to_currency(multiplier));
}
