#ifndef _CURRENCY_H_
#define _CURRENCY_H_

// Define the currency type
typedef unsigned long long currency_t;
currency_t float_to_currency(float amount);
double currency_to_float(currency_t amount);
currency_t fractional_price(currency_t price, float multiplier);
void currency_to_string(char *buffer, unsigned short buffer_size, currency_t amount);
void currency_to_string_extra(char *buffer, unsigned short buffer_size, currency_t amount, unsigned short flag);

#endif // _CURRENCY_H_

