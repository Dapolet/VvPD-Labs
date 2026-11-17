#include "fixed.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(condition, message)                    \
    do {                                             \
        if (!(condition)) {                          \
            printf("FAIL: %s\n", (message));         \
            ++failures;                              \
        }                                            \
    } while (0)

static void print_money_line(const char *label, Money value)
{
    printf("%s", label);
    money_print(stdout, value);
    printf(" (raw = %lld)\n", (long long)value.raw);
}

int main(void)
{
    printf("=== Money: масштаб 100 ===\n");
    Money a = money_from_cents(12345);
    Money b = money_from_raw(55);
    print_money_line("a = ", a);
    print_money_line("b = ", b);

    Money sum = money_add(a, b);
    Money difference = money_sub(a, b);
    Money negative = money_from_raw(-505);
    Money multiplied = money_mul_int(a, 3);

    print_money_line("a + b = ", sum);
    print_money_line("a - b = ", difference);
    print_money_line("-5.05 = ", negative);
    print_money_line("a * 3 = ", multiplied);

    Money parsed;
    if (money_parse("-12.34", &parsed))
        print_money_line("parse(\"-12.34\") = ", parsed);
    else
        printf("parse(\"-12.34\") failed\n");

    printf("parse(\"12.345\") accepted: %s\n",
           money_parse("12.345", &parsed) ? "yes" : "no");

    Money tenth = money_from_double(0.1);
    Money fifth = money_from_double(0.2);
    Money cents_sum = money_add(tenth, fifth);
    print_money_line("money(0.1) = ", tenth);
    print_money_line("money(0.2) = ", fifth);
    print_money_line("money(0.1) + money(0.2) = ", cents_sum);
    CHECK(cents_sum.raw == 30, "money 0.1 + 0.2 must be exactly 0.30");

    printf("0.1 + 0.2 in double = %.17g (equal to 0.3: %s)\n",
           0.1 + 0.2, (0.1 + 0.2 == 0.3) ? "yes" : "no");

    printf("\n=== Q16.16: double -> Q16.16 -> double ===\n");
    const double values[] = {
        0.0, 1.0, -1.0, 0.1, 0.2, 1.5,
        32767.9999, 32768.0, -32768.0, -32769.0
    };

    for (size_t i = 0; i < sizeof values / sizeof values[0]; ++i) {
        Q16 converted = q16_from_double(values[i]);
        printf("double %10.4f -> ", values[i]);
        q16_print(stdout, converted);
        printf(" -> double %.9f\n", q16_to_double(converted));
    }

    Q16 q_tenth = q16_from_double(0.1);
    Q16 q_fifth = q16_from_double(0.2);
    Q16 q_sum = q16_add(q_tenth, q_fifth);
    Q16 q_three_tenths = q16_from_double(0.3);

    printf("\nQ16(0.1) = "); q16_print(stdout, q_tenth); printf("\n");
    printf("Q16(0.2) = "); q16_print(stdout, q_fifth); printf("\n");
    printf("Q16(0.1) + Q16(0.2) = "); q16_print(stdout, q_sum); printf("\n");
    printf("Q16(0.3) = "); q16_print(stdout, q_three_tenths); printf("\n");
    CHECK(q_sum.raw == q_three_tenths.raw,
          "Q16 0.1 + 0.2 must equal Q16 0.3");

    Q16 q_one_and_half = q16_from_double(1.5);
    Q16 q_two = q16_from_double(2.0);
    Q16 q_product = q16_mul(q_one_and_half, q_two);
    printf("Q16(1.5) * Q16(2.0) = ");
    q16_print(stdout, q_product);
    printf("\n");
    CHECK(q16_to_double(q_product) > 2.9999 &&
          q16_to_double(q_product) < 3.0001,
          "Q16 multiplication 1.5 * 2.0 must be near 3.0");

    CHECK(q16_from_double(32768.0).raw == INT32_MAX,
          "Q16 must saturate above maximum");
    CHECK(q16_from_double(-32769.0).raw == INT32_MIN,
          "Q16 must saturate below minimum");

    printf("\nfailures = %d\n", failures);
    return failures == 0 ? 0 : 1;
}
