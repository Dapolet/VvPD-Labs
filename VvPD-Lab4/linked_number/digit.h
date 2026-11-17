#ifndef DIGIT_H
#define DIGIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Digit {
    uint8_t value;
    struct Digit *prev;
    struct Digit *next;
} Digit;

typedef struct {
    Digit *head;
    Digit *tail;
    size_t length;
} DecimalNumber;

void decimal_init(DecimalNumber *number);
void decimal_free(DecimalNumber *number);
bool decimal_push_front(DecimalNumber *number, uint8_t value);
bool decimal_push_back(DecimalNumber *number, uint8_t value);
bool decimal_from_string(DecimalNumber *number, const char *text);
void decimal_print_forward(const DecimalNumber *number, FILE *out);
void decimal_print_backward(const DecimalNumber *number, FILE *out);
bool decimal_sum_digits(const DecimalNumber *number, unsigned *sum);
size_t decimal_node_size(void);

#endif
