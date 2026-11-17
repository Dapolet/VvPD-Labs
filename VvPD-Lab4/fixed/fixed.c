#include "fixed.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>

Money money_from_cents(int64_t cents)
{
    Money result = {cents};
    return result;
}

Money money_from_raw(int64_t raw)
{
    Money result = {raw};
    return result;
}

Money money_from_double(double units)
{
    long double scaled = (long double)units * (long double)MONEY_SCALE;
    int64_t raw;

    if (scaled >= (long double)INT64_MAX)
        raw = INT64_MAX;
    else if (scaled <= (long double)INT64_MIN)
        raw = INT64_MIN;
    else
        raw = (int64_t)llroundl(scaled);

    return money_from_raw(raw);
}

bool money_parse(const char *text, Money *out)
{
    if (text == NULL || out == NULL)
        return false;

    const char *p = text;
    bool negative = false;

    if (*p == '+' || *p == '-') {
        negative = (*p == '-');
        ++p;
    }

    if (!isdigit((unsigned char)*p))
        return false;

    uint64_t whole = 0;
    while (isdigit((unsigned char)*p)) {
        unsigned digit = (unsigned)(*p - '0');
        if (whole > (UINT64_MAX - digit) / 10)
            return false;
        whole = whole * 10 + digit;
        ++p;
    }

    uint64_t fraction = 0;
    int fraction_digits = 0;

    if (*p == '.') {
        ++p;
        if (!isdigit((unsigned char)*p))
            return false;

        while (isdigit((unsigned char)*p)) {
            if (fraction_digits >= 2)
                return false;
            fraction = fraction * 10 + (unsigned)(*p - '0');
            ++fraction_digits;
            ++p;
        }
    }

    if (*p != '\0')
        return false;

    while (fraction_digits < 2) {
        fraction *= 10;
        ++fraction_digits;
    }

    if (whole > (UINT64_MAX - fraction) / MONEY_SCALE)
        return false;

    uint64_t cents = whole * MONEY_SCALE + fraction;

    if (negative) {
        const uint64_t limit = (uint64_t)INT64_MAX + 1u;
        if (cents > limit)
            return false;
        out->raw = cents == limit ? INT64_MIN : -(int64_t)cents;
    }
    else {
        if (cents > (uint64_t)INT64_MAX)
            return false;
        out->raw = (int64_t)cents;
    }

    return true;
}

Money money_add(Money a, Money b)
{
    return money_from_raw(a.raw + b.raw);
}

Money money_sub(Money a, Money b)
{
    return money_from_raw(a.raw - b.raw);
}

Money money_mul_int(Money a, int64_t factor)
{
    return money_from_raw(a.raw * factor);
}

void money_print(FILE *out, Money value)
{
    bool negative = value.raw < 0;
    uint64_t magnitude = negative
        ? (uint64_t)(-(value.raw + 1)) + 1u
        : (uint64_t)value.raw;

    fprintf(out, "%s%llu.%02llu",
            negative ? "-" : "",
            (unsigned long long)(magnitude / MONEY_SCALE),
            (unsigned long long)(magnitude % MONEY_SCALE));
}

Q16 q16_from_double(double value)
{
    long double scaled = (long double)value * (long double)Q16_ONE;
    int32_t raw;

    if (scaled >= (long double)INT32_MAX)
        raw = INT32_MAX;
    else if (scaled <= (long double)INT32_MIN)
        raw = INT32_MIN;
    else
        raw = (int32_t)llroundl(scaled);

    Q16 result = {raw};
    return result;
}

double q16_to_double(Q16 value)
{
    return (double)value.raw / (double)Q16_ONE;
}

Q16 q16_add(Q16 a, Q16 b)
{
    int64_t sum = (int64_t)a.raw + (int64_t)b.raw;
    int32_t raw;

    if (sum > INT32_MAX)
        raw = INT32_MAX;
    else if (sum < INT32_MIN)
        raw = INT32_MIN;
    else
        raw = (int32_t)sum;

    Q16 result = {raw};
    return result;
}

Q16 q16_sub(Q16 a, Q16 b)
{
    int64_t difference = (int64_t)a.raw - (int64_t)b.raw;
    int32_t raw;

    if (difference > INT32_MAX)
        raw = INT32_MAX;
    else if (difference < INT32_MIN)
        raw = INT32_MIN;
    else
        raw = (int32_t)difference;

    Q16 result = {raw};
    return result;
}

Q16 q16_mul(Q16 a, Q16 b)
{
    int64_t product = (int64_t)a.raw * (int64_t)b.raw;
    int64_t rounded = product >= 0
        ? (product + Q16_ONE / 2) / Q16_ONE
        : (product - Q16_ONE / 2) / Q16_ONE;

    int32_t raw;
    if (rounded > INT32_MAX)
        raw = INT32_MAX;
    else if (rounded < INT32_MIN)
        raw = INT32_MIN;
    else
        raw = (int32_t)rounded;

    Q16 result = {raw};
    return result;
}

void q16_print(FILE *out, Q16 value)
{
    fprintf(out, "raw=%d (0x%08x), value=%.9f",
            value.raw,
            (unsigned)value.raw,
            q16_to_double(value));
}
