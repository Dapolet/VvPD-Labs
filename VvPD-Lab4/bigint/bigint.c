#include "bigint.h"

#include <stdlib.h>
#include <string.h>

static const uint64_t LIMB_BASE = 1ULL << 32;

void biguint_init(BigUInt *value)
{
    if (value == NULL)
        return;
    value->limbs = NULL;
    value->size = 0;
    value->capacity = 0;
}

void biguint_free(BigUInt *value)
{
    if (value == NULL)
        return;
    free(value->limbs);
    value->limbs = NULL;
    value->size = 0;
    value->capacity = 0;
}

bool biguint_reserve(BigUInt *value, size_t capacity)
{
    if (value == NULL)
        return false;
    if (capacity <= value->capacity)
        return true;
    if (capacity > SIZE_MAX / sizeof *value->limbs)
        return false;

    size_t old_capacity = value->capacity;
    uint32_t *temporary = realloc(value->limbs, capacity * sizeof *temporary);
    if (temporary == NULL)
        return false;

    memset(temporary + old_capacity, 0,
           (capacity - old_capacity) * sizeof *temporary);

    value->limbs = temporary;
    value->capacity = capacity;
    return true;
}

void biguint_normalize(BigUInt *value)
{
    if (value == NULL)
        return;

    while (value->size > 0 && value->limbs[value->size - 1] == 0)
        --value->size;
}

bool biguint_from_u64(BigUInt *value, uint64_t number)
{
    if (value == NULL)
        return false;

    if (number == 0) {
        value->size = 0;
        return true;
    }

    if (!biguint_reserve(value, 2))
        return false;

    value->limbs[0] = (uint32_t)number;
    value->limbs[1] = (uint32_t)(number >> 32);
    value->size = 2;
    biguint_normalize(value);
    return true;
}

bool biguint_from_hex(BigUInt *value, const char *hex)
{
    if (value == NULL || hex == NULL)
        return false;

    const char *digits = hex;
    if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
        digits += 2;

    size_t count = strlen(digits);
    if (count == 0)
        return false;

    for (size_t i = 0; i < count; ++i) {
        if (!((digits[i] >= '0' && digits[i] <= '9') ||
              (digits[i] >= 'a' && digits[i] <= 'f') ||
              (digits[i] >= 'A' && digits[i] <= 'F')))
            return false;
    }

    size_t limb_count = (count + 7) / 8;
    if (!biguint_reserve(value, limb_count))
        return false;

    memset(value->limbs, 0, limb_count * sizeof *value->limbs);

    for (size_t i = 0; i < count; ++i) {
        char c = digits[count - 1 - i];
        uint32_t nibble;

        if (c >= '0' && c <= '9')
            nibble = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f')
            nibble = (uint32_t)(c - 'a' + 10);
        else
            nibble = (uint32_t)(c - 'A' + 10);

        size_t limb = i / 8;
        unsigned shift = (unsigned)((i % 8) * 4);
        value->limbs[limb] |= nibble << shift;
    }

    value->size = limb_count;
    biguint_normalize(value);
    return true;
}

bool biguint_copy(BigUInt *destination, const BigUInt *source)
{
    if (destination == NULL || source == NULL)
        return false;
    if (destination == source)
        return true;

    if (!biguint_reserve(destination, source->size))
        return false;

    if (source->size > 0) {
        memcpy(destination->limbs, source->limbs,
               source->size * sizeof *destination->limbs);
    }
    destination->size = source->size;
    return true;
}

int biguint_compare(const BigUInt *left, const BigUInt *right)
{
    if (left->size != right->size)
        return left->size < right->size ? -1 : 1;

    for (size_t i = left->size; i > 0; --i) {
        uint32_t a = left->limbs[i - 1];
        uint32_t b = right->limbs[i - 1];
        if (a != b)
            return a < b ? -1 : 1;
    }

    return 0;
}

bool biguint_is_zero(const BigUInt *value)
{
    return value == NULL || value->size == 0;
}

bool biguint_add(BigUInt *result, const BigUInt *left, const BigUInt *right)
{
    if (result == NULL || left == NULL || right == NULL)
        return false;

    size_t count = left->size > right->size ? left->size : right->size;
    uint32_t *temporary = calloc(count + 1, sizeof *temporary);
    if (temporary == NULL)
        return false;

    uint64_t carry = 0;
    for (size_t i = 0; i < count; ++i) {
        uint64_t sum = carry;
        if (i < left->size)
            sum += left->limbs[i];
        if (i < right->size)
            sum += right->limbs[i];

        temporary[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    temporary[count] = (uint32_t)carry;

    size_t size = count + 1;
    while (size > 0 && temporary[size - 1] == 0)
        --size;

    free(result->limbs);
    result->limbs = temporary;
    result->size = size;
    result->capacity = count + 1;

    if (size == 0) {
        free(result->limbs);
        result->limbs = NULL;
        result->capacity = 0;
    }

    return true;
}

bool biguint_sub(BigUInt *result, const BigUInt *left, const BigUInt *right)
{
    if (result == NULL || left == NULL || right == NULL)
        return false;
    if (biguint_compare(left, right) < 0)
        return false;

    if (left->size == 0) {
        biguint_free(result);
        return true;
    }

    uint32_t *temporary = calloc(left->size, sizeof *temporary);
    if (temporary == NULL)
        return false;

    uint64_t borrow = 0;
    for (size_t i = 0; i < left->size; ++i) {
        uint64_t subtrahend = borrow;
        if (i < right->size)
            subtrahend += right->limbs[i];

        uint64_t minuend = left->limbs[i];
        if (minuend >= subtrahend) {
            temporary[i] = (uint32_t)(minuend - subtrahend);
            borrow = 0;
        }
        else {
            temporary[i] = (uint32_t)(minuend + LIMB_BASE - subtrahend);
            borrow = 1;
        }
    }

    size_t size = left->size;
    while (size > 0 && temporary[size - 1] == 0)
        --size;

    free(result->limbs);
    result->limbs = temporary;
    result->size = size;
    result->capacity = left->size;

    if (size == 0) {
        free(result->limbs);
        result->limbs = NULL;
        result->capacity = 0;
    }

    return true;
}

bool biguint_mul(BigUInt *result, const BigUInt *left, const BigUInt *right)
{
    if (result == NULL || left == NULL || right == NULL)
        return false;

    if (left->size == 0 || right->size == 0) {
        biguint_free(result);
        return true;
    }

    if (left->size > SIZE_MAX - right->size - 1)
        return false;

    size_t count = left->size + right->size + 1;
    uint32_t *temporary = calloc(count, sizeof *temporary);
    if (temporary == NULL)
        return false;

    for (size_t i = 0; i < left->size; ++i) {
        uint64_t carry = 0;

        for (size_t j = 0; j < right->size; ++j) {
            uint64_t current = (uint64_t)temporary[i + j] +
                               (uint64_t)left->limbs[i] * right->limbs[j] +
                               carry;
            temporary[i + j] = (uint32_t)current;
            carry = current >> 32;
        }

        size_t k = i + right->size;
        while (carry != 0 && k < count) {
            uint64_t current = (uint64_t)temporary[k] + carry;
            temporary[k] = (uint32_t)current;
            carry = current >> 32;
            ++k;
        }
    }

    size_t size = count;
    while (size > 0 && temporary[size - 1] == 0)
        --size;

    free(result->limbs);
    result->limbs = temporary;
    result->size = size;
    result->capacity = count;

    if (size == 0) {
        free(result->limbs);
        result->limbs = NULL;
        result->capacity = 0;
    }

    return true;
}

bool biguint_mul_u64(BigUInt *result, const BigUInt *value, uint64_t factor)
{
    BigUInt multiplier;
    biguint_init(&multiplier);

    if (!biguint_from_u64(&multiplier, factor)) {
        biguint_free(&multiplier);
        return false;
    }

    bool ok = biguint_mul(result, value, &multiplier);
    biguint_free(&multiplier);
    return ok;
}

bool biguint_pow2(BigUInt *result, size_t exponent)
{
    if (result == NULL)
        return false;

    size_t limb_index = exponent / 32;
    unsigned bit_index = (unsigned)(exponent % 32);

    if (limb_index == SIZE_MAX)
        return false;

    size_t count = limb_index + 1;
    uint32_t *temporary = calloc(count, sizeof *temporary);
    if (temporary == NULL)
        return false;

    temporary[limb_index] = (uint32_t)1u << bit_index;

    free(result->limbs);
    result->limbs = temporary;
    result->size = count;
    result->capacity = count;
    return true;
}

bool biguint_to_u64(const BigUInt *value, uint64_t *out)
{
    if (value == NULL || out == NULL || value->size > 2)
        return false;

    uint64_t result = 0;
    for (size_t i = 0; i < value->size; ++i)
        result |= (uint64_t)value->limbs[i] << (32 * i);

    *out = result;
    return true;
}

size_t biguint_bit_length(const BigUInt *value)
{
    if (value == NULL || value->size == 0)
        return 0;

    uint32_t top = value->limbs[value->size - 1];
    size_t bits = 0;
    while (top != 0) {
        ++bits;
        top >>= 1;
    }

    return (value->size - 1) * 32 + bits;
}

bool biguint_divmod_u32(BigUInt *value, uint32_t divisor, uint32_t *remainder)
{
    if (value == NULL || divisor == 0)
        return false;

    uint64_t rem = 0;
    for (size_t i = value->size; i > 0; --i) {
        uint64_t current = (rem << 32) | value->limbs[i - 1];
        value->limbs[i - 1] = (uint32_t)(current / divisor);
        rem = current % divisor;
    }

    if (remainder != NULL)
        *remainder = (uint32_t)rem;

    biguint_normalize(value);
    return true;
}

void biguint_print_hex(FILE *out, const BigUInt *value)
{
    if (out == NULL || value == NULL)
        return;

    if (value->size == 0) {
        fputc('0', out);
        return;
    }

    fprintf(out, "%x", value->limbs[value->size - 1]);
    for (size_t i = value->size - 1; i > 0; --i)
        fprintf(out, "%08x", value->limbs[i - 1]);
}

void biguint_print_dec(FILE *out, const BigUInt *value)
{
    if (out == NULL || value == NULL)
        return;

    if (value->size == 0) {
        fputc('0', out);
        return;
    }

    size_t capacity = value->size * 2 + 4;
    uint32_t *chunks = malloc(capacity * sizeof *chunks);
    if (chunks == NULL) {
        fputs("<out of memory>", out);
        return;
    }

    BigUInt temporary;
    biguint_init(&temporary);
    if (!biguint_copy(&temporary, value)) {
        free(chunks);
        fputs("<out of memory>", out);
        return;
    }

    size_t chunk_count = 0;
    while (temporary.size > 0) {
        uint32_t remainder = 0;
        if (!biguint_divmod_u32(&temporary, 1000000000u, &remainder)) {
            biguint_free(&temporary);
            free(chunks);
            fputs("<division error>", out);
            return;
        }

        if (chunk_count == capacity) {
            size_t new_capacity = capacity * 2;
            uint32_t *grown = realloc(chunks, new_capacity * sizeof *grown);
            if (grown == NULL) {
                biguint_free(&temporary);
                free(chunks);
                fputs("<out of memory>", out);
                return;
            }
            chunks = grown;
            capacity = new_capacity;
        }

        chunks[chunk_count++] = remainder;
    }

    biguint_free(&temporary);

    if (chunk_count == 0) {
        free(chunks);
        fputc('0', out);
        return;
    }

    fprintf(out, "%u", chunks[chunk_count - 1]);
    for (size_t i = chunk_count - 1; i > 0; --i)
        fprintf(out, "%09u", chunks[i - 1]);

    free(chunks);
}
