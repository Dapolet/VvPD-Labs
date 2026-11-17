#ifndef BIGINT_H
#define BIGINT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Little-endian: limbs[0] — младшие 32 бита.
 * Ноль представляется как size == 0.
 */
typedef struct {
    uint32_t *limbs;
    size_t size;
    size_t capacity;
} BigUInt;

void biguint_init(BigUInt *value);
void biguint_free(BigUInt *value);
bool biguint_reserve(BigUInt *value, size_t capacity);
void biguint_normalize(BigUInt *value);
bool biguint_from_u64(BigUInt *value, uint64_t number);
bool biguint_from_hex(BigUInt *value, const char *hex);
bool biguint_copy(BigUInt *destination, const BigUInt *source);
int biguint_compare(const BigUInt *left, const BigUInt *right);
bool biguint_is_zero(const BigUInt *value);
bool biguint_add(BigUInt *result, const BigUInt *left, const BigUInt *right);
bool biguint_sub(BigUInt *result, const BigUInt *left, const BigUInt *right);
bool biguint_mul(BigUInt *result, const BigUInt *left, const BigUInt *right);
bool biguint_mul_u64(BigUInt *result, const BigUInt *value, uint64_t factor);
bool biguint_pow2(BigUInt *result, size_t exponent);
bool biguint_to_u64(const BigUInt *value, uint64_t *out);
size_t biguint_bit_length(const BigUInt *value);
bool biguint_divmod_u32(BigUInt *value, uint32_t divisor, uint32_t *remainder);
void biguint_print_hex(FILE *out, const BigUInt *value);
void biguint_print_dec(FILE *out, const BigUInt *value);

#endif
