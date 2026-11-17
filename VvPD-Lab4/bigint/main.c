#include "bigint.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static char *capture_hex(const BigUInt *value)
{
    FILE *file = tmpfile();
    if (file == NULL)
        return NULL;

    biguint_print_hex(file, value);
    if (fflush(file) != 0) {
        fclose(file);
        return NULL;
    }

    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        return NULL;
    }

    rewind(file);
    char *buffer = malloc((size_t)length + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t got = fread(buffer, 1, (size_t)length, file);
    fclose(file);
    if (got != (size_t)length) {
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

static char *capture_dec(const BigUInt *value)
{
    FILE *file = tmpfile();
    if (file == NULL)
        return NULL;

    biguint_print_dec(file, value);
    if (fflush(file) != 0) {
        fclose(file);
        return NULL;
    }

    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        return NULL;
    }

    rewind(file);
    char *buffer = malloc((size_t)length + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t got = fread(buffer, 1, (size_t)length, file);
    fclose(file);
    if (got != (size_t)length) {
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

static void check_hex(const char *label, const BigUInt *value,
                      const char *expected)
{
    char *actual = capture_hex(value);
    if (actual == NULL) {
        printf("FAIL %-24s: cannot format value\n", label);
        ++failures;
        return;
    }

    if (strcmp(actual, expected) != 0) {
        printf("FAIL %-24s: expected %s, got %s\n",
               label, expected, actual);
        ++failures;
    }
    else {
        printf("PASS %-24s: %s\n", label, actual);
    }

    free(actual);
}

static void check_dec(const char *label, const BigUInt *value,
                      const char *expected)
{
    char *actual = capture_dec(value);
    if (actual == NULL) {
        printf("FAIL %-24s: cannot format value\n", label);
        ++failures;
        return;
    }

    if (strcmp(actual, expected) != 0) {
        printf("FAIL %-24s: decimal mismatch\n", label);
        ++failures;
    }
    else {
        printf("PASS %-24s: decimal value matches\n", label);
    }

    free(actual);
}

static void check_power_of_two(size_t exponent)
{
    BigUInt value;
    biguint_init(&value);

    printf("\n2^%zu:\n", exponent);
    if (!biguint_pow2(&value, exponent)) {
        printf("FAIL: biguint_pow2(%zu)\n", exponent);
        ++failures;
        biguint_free(&value);
        return;
    }

    char *hex = capture_hex(&value);
    if (hex == NULL) {
        puts("FAIL: cannot format power of two");
        ++failures;
        biguint_free(&value);
        return;
    }

    size_t length = strlen(hex);
    size_t expected_length = exponent / 4 + 1;
    bool ok = length == expected_length && hex[0] == '1';
    for (size_t i = 1; ok && i < length; ++i) {
        if (hex[i] != '0')
            ok = false;
    }

    printf("bit_length = %zu (expected %zu)\n",
           biguint_bit_length(&value), exponent + 1);
    printf("hex length = %zu (expected %zu)\n", length, expected_length);
    printf("hex = %s\n", hex);

    if (!ok) {
        puts("FAIL: power of two has unexpected hex form");
        ++failures;
    }
    else if (biguint_bit_length(&value) != exponent + 1) {
        puts("FAIL: power of two has unexpected bit length");
        ++failures;
    }

    free(hex);
    biguint_free(&value);
}

static void test_addition(void)
{
    puts("=== addition ===");
    BigUInt a, b, result;
    biguint_init(&a);
    biguint_init(&b);
    biguint_init(&result);

    biguint_from_u64(&a, 0);
    biguint_from_u64(&b, 0);
    biguint_add(&result, &a, &b);
    check_hex("0 + 0", &result, "0");

    biguint_from_u64(&a, 1);
    biguint_from_u64(&b, 1);
    biguint_add(&result, &a, &b);
    check_hex("1 + 1", &result, "2");

    biguint_from_u64(&a, 0xFFFFFFFFULL);
    biguint_from_u64(&b, 1);
    biguint_add(&result, &a, &b);
    check_hex("0xFFFFFFFF + 1", &result, "100000000");

    biguint_from_u64(&a, 0xFFFFFFFFFFFFFFFFULL);
    biguint_from_u64(&b, 1);
    biguint_add(&result, &a, &b);
    check_hex("0xFFFFFFFFFFFFFFFF + 1", &result, "10000000000000000");

    biguint_from_hex(&a, "123456789abcdef");
    biguint_from_u64(&b, 1);
    biguint_add(&result, &a, &b);
    check_hex("different sizes", &result, "123456789abcdf0");

    biguint_from_u64(&a, 12345);
    biguint_from_u64(&b, 0);
    biguint_add(&result, &a, &b);
    check_hex("number + zero", &result, "3039");

    biguint_from_u64(&a, 0xFFFFFFFFULL);
    biguint_from_u64(&b, 1);
    biguint_add(&a, &a, &b);
    check_hex("alias result == left", &a, "100000000");

    biguint_from_u64(&a, 0xFFFFFFFFFFFFFFFFULL);
    biguint_from_u64(&b, 1);
    biguint_free(&result);
    biguint_init(&result);
    biguint_reserve(&result, 1);
    biguint_add(&result, &a, &b);
    check_hex("after reserve(capacity=1)", &result, "10000000000000000");

    biguint_free(&a);
    biguint_free(&b);
    biguint_free(&result);
}

static void test_subtraction(void)
{
    puts("\n=== subtraction ===");
    BigUInt a, b, result;
    biguint_init(&a);
    biguint_init(&b);
    biguint_init(&result);

    biguint_from_u64(&a, 0x100000000ULL);
    biguint_from_u64(&b, 1);
    if (!biguint_sub(&result, &a, &b)) {
        puts("FAIL: 0x100000000 - 1 returned false");
        ++failures;
    }
    else {
        check_hex("0x100000000 - 1", &result, "ffffffff");
    }

    biguint_from_u64(&a, 12345);
    biguint_from_u64(&b, 12345);
    biguint_sub(&result, &a, &b);
    check_hex("equal numbers - equal", &result, "0");

    biguint_from_u64(&a, 1);
    biguint_from_u64(&b, 2);
    if (biguint_sub(&result, &a, &b)) {
        puts("FAIL: 1 - 2 must return false");
        ++failures;
    }
    else {
        puts("PASS: 1 - 2 rejected (a >= b required)");
    }

    biguint_from_hex(&a, "123456789abcdef");
    biguint_from_hex(&b, "123456789abcdee");
    biguint_sub(&result, &a, &b);
    check_hex("borrow between limbs", &result, "1");

    biguint_from_u64(&a, 10);
    biguint_from_u64(&b, 3);
    biguint_sub(&a, &a, &b);
    check_hex("alias result == left", &a, "7");

    biguint_from_u64(&a, 0);
    biguint_from_u64(&b, 0);
    biguint_sub(&result, &a, &b);
    check_hex("0 - 0", &result, "0");

    biguint_free(&a);
    biguint_free(&b);
    biguint_free(&result);
}

static void test_multiplication(void)
{
    puts("\n=== multiplication ===");
    BigUInt a, b, result;
    biguint_init(&a);
    biguint_init(&b);
    biguint_init(&result);

    biguint_from_u64(&a, 0xFFFFFFFFULL);
    biguint_from_u64(&b, 0xFFFFFFFFULL);
    biguint_mul(&result, &a, &b);
    check_hex("0xFFFFFFFF * 0xFFFFFFFF", &result, "fffffffe00000001");

    biguint_from_u64(&a, 0x100000000ULL);
    biguint_from_u64(&b, 0x100000000ULL);
    biguint_mul(&result, &a, &b);
    check_hex("2^32 * 2^32", &result, "10000000000000000");

    biguint_from_u64(&a, 123456789ULL);
    biguint_from_u64(&b, 987654321ULL);
    biguint_mul(&result, &a, &b);
    check_dec("123456789 * 987654321", &result, "121932631112635269");

    biguint_from_u64(&a, 0);
    biguint_from_u64(&b, 12345);
    biguint_mul(&result, &a, &b);
    check_hex("zero * number", &result, "0");

    biguint_from_u64(&a, 0xFFFFFFFFULL);
    biguint_from_u64(&b, 0xFFFFFFFFULL);
    biguint_mul(&a, &a, &b);
    check_hex("alias result == left", &a, "fffffffe00000001");

    biguint_free(&a);
    biguint_free(&b);
    biguint_free(&result);
}

static void test_compare_and_convert(void)
{
    puts("\n=== compare, normalize, conversion ===");
    BigUInt a, b;
    biguint_init(&a);
    biguint_init(&b);

    biguint_from_u64(&a, 1);
    biguint_from_u64(&b, 2);
    if (biguint_compare(&a, &b) >= 0) {
        puts("FAIL: 1 must be less than 2");
        ++failures;
    }
    else {
        puts("PASS: compare(1, 2) < 0");
    }

    biguint_from_u64(&a, 2);
    biguint_from_u64(&b, 2);
    if (biguint_compare(&a, &b) != 0) {
        puts("FAIL: equal numbers must compare equal");
        ++failures;
    }
    else {
        puts("PASS: compare(2, 2) == 0");
    }

    biguint_from_hex(&a, "0000000000000001");
    check_hex("normalize leading zeros", &a, "1");

    biguint_from_u64(&a, 0);
    if (!biguint_is_zero(&a)) {
        puts("FAIL: zero must be detected");
        ++failures;
    }
    else {
        puts("PASS: zero detected");
    }

    uint64_t number = 0;
    biguint_from_u64(&a, 0x123456789abcdef0ULL);
    if (!biguint_to_u64(&a, &number) || number != 0x123456789abcdef0ULL) {
        puts("FAIL: to_u64 round-trip");
        ++failures;
    }
    else {
        printf("PASS: to_u64 = 0x%016llx\n", (unsigned long long)number);
    }

    biguint_free(&a);
    biguint_free(&b);
}

static bool factorial(BigUInt *result, unsigned n)
{
    if (!biguint_from_u64(result, 1))
        return false;

    for (unsigned i = 2; i <= n; ++i) {
        BigUInt next;
        biguint_init(&next);

        if (!biguint_mul_u64(&next, result, i)) {
            biguint_free(&next);
            return false;
        }

        biguint_free(result);
        *result = next;
    }

    return true;
}

static void test_factorial(void)
{
    puts("\n=== additional task: factorial ===");

    BigUInt value;
    biguint_init(&value);

    if (!factorial(&value, 100)) {
        puts("FAIL: cannot compute 100!");
        ++failures;
        return;
    }

    printf("100! bit length = %zu\n", biguint_bit_length(&value));
    printf("100! = ");
    biguint_print_dec(stdout, &value);
    printf("\n");

    check_dec("100!", &value,
              "93326215443944152681699238856266700490715968264381621468592963895217599993229915608941463976156518286253697920827223758251185210916864000000000000000000000000");
    if (biguint_bit_length(&value) != 525) {
        puts("FAIL: 100! bit length must be 525");
        ++failures;
    }

    biguint_free(&value);
    biguint_init(&value);

    if (!factorial(&value, 1000)) {
        puts("FAIL: cannot compute 1000!");
        ++failures;
        return;
    }

    char *decimal = capture_dec(&value);
    if (decimal == NULL) {
        puts("FAIL: cannot format 1000!");
        ++failures;
    }
    else {
        size_t length = strlen(decimal);
        printf("\n1000! decimal length = %zu, bit length = %zu\n",
               length, biguint_bit_length(&value));
        printf("1000! first 40 digits = %.40s\n", decimal);
        printf("1000! last 20 digits  = %s\n", decimal + length - 20);

        if (length != 2568) {
            puts("FAIL: 1000! must have 2568 decimal digits");
            ++failures;
        }
        if (biguint_bit_length(&value) != 8530) {
            puts("FAIL: 1000! bit length must be 8530");
            ++failures;
        }
        for (size_t i = length - 20; i < length; ++i) {
            if (decimal[i] != '0') {
                puts("FAIL: 1000! must end with at least 20 zeros");
                ++failures;
                break;
            }
        }

        free(decimal);
    }

    biguint_free(&value);
}

int main(void)
{
    puts("=== BigUInt tests ===");

    test_addition();
    test_subtraction();
    test_multiplication();
    test_compare_and_convert();

    puts("\n=== powers of two ===");
    check_power_of_two(304);
    check_power_of_two(4096);
    check_power_of_two(16384);

    test_factorial();

    printf("\nfailures = %d\n", failures);
    return failures == 0 ? 0 : 1;
}
