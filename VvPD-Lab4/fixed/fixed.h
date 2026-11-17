#ifndef FIXED_H
#define FIXED_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* Денежный тип: сырое значение хранится в сотых долях. */
typedef struct {
    int64_t raw;
} Money;

/* Q16.16: 16 бит целой части, 16 бит дробной. */
typedef struct {
    int32_t raw;
} Q16;

#define MONEY_SCALE 100
#define Q16_FRAC_BITS 16
#define Q16_ONE ((int32_t)1 << Q16_FRAC_BITS)

Money money_from_cents(int64_t cents);
Money money_from_raw(int64_t raw);
Money money_from_double(double units);
bool money_parse(const char *text, Money *out);
Money money_add(Money a, Money b);
Money money_sub(Money a, Money b);
Money money_mul_int(Money a, int64_t factor);
void money_print(FILE *out, Money value);

Q16 q16_from_double(double value);
double q16_to_double(Q16 value);
Q16 q16_add(Q16 a, Q16 b);
Q16 q16_sub(Q16 a, Q16 b);
Q16 q16_mul(Q16 a, Q16 b);
void q16_print(FILE *out, Q16 value);

#endif
