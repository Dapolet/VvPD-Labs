#include "digit.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void decimal_init(DecimalNumber *number)
{
    if (number == NULL)
        return;
    number->head = NULL;
    number->tail = NULL;
    number->length = 0;
}

static Digit *digit_create(uint8_t value)
{
    Digit *digit = malloc(sizeof *digit);
    if (digit == NULL)
        return NULL;

    digit->value = value;
    digit->prev = NULL;
    digit->next = NULL;
    return digit;
}

bool decimal_push_back(DecimalNumber *number, uint8_t value)
{
    if (number == NULL || value > 9)
        return false;

    Digit *digit = digit_create(value);
    if (digit == NULL)
        return false;

    digit->prev = number->tail;
    if (number->tail != NULL)
        number->tail->next = digit;
    else
        number->head = digit;
    number->tail = digit;
    ++number->length;
    return true;
}

bool decimal_push_front(DecimalNumber *number, uint8_t value)
{
    if (number == NULL || value > 9)
        return false;

    Digit *digit = digit_create(value);
    if (digit == NULL)
        return false;

    digit->next = number->head;
    if (number->head != NULL)
        number->head->prev = digit;
    else
        number->tail = digit;
    number->head = digit;
    ++number->length;
    return true;
}

bool decimal_from_string(DecimalNumber *number, const char *text)
{
    if (number == NULL || text == NULL || *text == '\0')
        return false;

    decimal_free(number);
    decimal_init(number);

    for (const char *p = text; *p != '\0'; ++p) {
        if (!isdigit((unsigned char)*p)) {
            decimal_free(number);
            return false;
        }
        if (!decimal_push_back(number, (uint8_t)(*p - '0'))) {
            decimal_free(number);
            return false;
        }
    }

    return number->length > 0;
}

void decimal_print_forward(const DecimalNumber *number, FILE *out)
{
    if (number == NULL || out == NULL)
        return;

    for (const Digit *current = number->head; current != NULL;
         current = current->next) {
        fprintf(out, "%u", (unsigned)current->value);
    }
}

void decimal_print_backward(const DecimalNumber *number, FILE *out)
{
    if (number == NULL || out == NULL)
        return;

    for (const Digit *current = number->tail; current != NULL;
         current = current->prev) {
        fprintf(out, "%u", (unsigned)current->value);
    }
}

bool decimal_sum_digits(const DecimalNumber *number, unsigned *sum)
{
    if (number == NULL || sum == NULL)
        return false;

    unsigned total = 0;
    for (const Digit *current = number->head; current != NULL;
         current = current->next) {
        total += current->value;
    }

    *sum = total;
    return true;
}

void decimal_free(DecimalNumber *number)
{
    if (number == NULL)
        return;

    Digit *current = number->head;
    while (current != NULL) {
        Digit *next = current->next;
        free(current);
        current = next;
    }

    number->head = NULL;
    number->tail = NULL;
    number->length = 0;
}

size_t decimal_node_size(void)
{
    return sizeof(Digit);
}
