#include "digit.h"

#include <stdio.h>

int main(void)
{
    const char *text = "304159265358979323846264338327950288419716939937510";
    DecimalNumber number;
    decimal_init(&number);

    printf("=== двусвязное десятичное число ===\n");
    if (!decimal_from_string(&number, text)) {
        fputs("decimal_from_string failed\n", stderr);
        return 1;
    }

    printf("source: ");
    puts(text);
    printf("forward: ");
    decimal_print_forward(&number, stdout);
    printf("\nbackward: ");
    decimal_print_backward(&number, stdout);
    printf("\n");

    unsigned sum = 0;
    decimal_sum_digits(&number, &sum);
    printf("length = %zu, digit sum = %u\n", number.length, sum);

    printf("\nsizeof(Digit) = %zu bytes\n", decimal_node_size());
    printf("payload per digit = %zu byte(s)\n", sizeof(uint8_t));
    printf("nodes = %zu, total node bytes = %zu\n",
           number.length, number.length * decimal_node_size());
    printf("contiguous payload would be %zu byte(s)\n", number.length);

    printf("\n=== push_front(9), push_back(2) ===\n");
    if (!decimal_push_front(&number, 9) || !decimal_push_back(&number, 2)) {
        fputs("push failed\n", stderr);
        decimal_free(&number);
        return 1;
    }

    printf("forward: ");
    decimal_print_forward(&number, stdout);
    printf("\nbackward: ");
    decimal_print_backward(&number, stdout);
    printf("\nlength = %zu\n", number.length);

    printf("\n=== освобождение всех узлов ===\n");
    decimal_free(&number);
    printf("after free: head = %p, tail = %p, length = %zu\n",
           (void *)number.head, (void *)number.tail, number.length);

    decimal_free(&number);
    printf("second decimal_free is safe: head = %p, length = %zu\n",
           (void *)number.head, number.length);

    return 0;
}
