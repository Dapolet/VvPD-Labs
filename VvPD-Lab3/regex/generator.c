#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Лабораторная работа № 3, п. 3.2.
 *
 * Генератор большого потока цифр с персональной контрольной
 * последовательностью. Данные пишутся в stdout одной строкой,
 * поэтому используется перенаправление:
 *
 *     ./generator > VPD_Lab3.txt
 */

#define DIGITS 100000
#define CONTROL_REPEATS 3

/* Имя и фамилия латиницей: каждый байт — печатный ASCII. */
static const char FULL_NAME[] = "Dmitrii Poletaev";

static void fail(const char *message)
{
    fprintf(stderr, "generator: %s\n", message);
    exit(EXIT_FAILURE);
}

/* Десятичные коды ASCII-байтов, записанные подряд без разделителей. */
static size_t build_control(char *buffer, size_t capacity)
{
    size_t used = 0;

    for (const unsigned char *p = (const unsigned char *)FULL_NAME; *p != 0; ++p) {
        int written = snprintf(buffer + used, capacity - used, "%u", (unsigned int)*p);

        if (written < 0 || (size_t)written >= capacity - used) {
            fail("control sequence does not fit into the buffer");
        }
        used += (size_t)written;
    }

    buffer[used] = '\0';
    return used;
}

static size_t count_occurrences(const char *haystack, const char *needle)
{
    size_t count = 0;
    const char *cursor = haystack;

    while ((cursor = strstr(cursor, needle)) != NULL) {
        ++count;
        cursor += strlen(needle);
    }

    return count;
}

int main(void)
{
    char control[512];
    size_t control_length = build_control(control, sizeof(control));

    char *digits = malloc((size_t)DIGITS + 1);
    char *output = malloc((size_t)DIGITS + CONTROL_REPEATS * control_length + 2);

    if (digits == NULL || output == NULL) {
        fail("out of memory");
    }

    srand((unsigned int)time(NULL));
    for (size_t i = 0; i < DIGITS; ++i) {
        digits[i] = (char)('0' + rand() % 10);
    }

    size_t position = 0;
    const size_t middle = DIGITS / 2;

    /* Контрольная последовательность: начало, середина, конец. */
    memcpy(output + position, control, control_length);
    position += control_length;

    memcpy(output + position, digits, middle);
    position += middle;

    memcpy(output + position, control, control_length);
    position += control_length;

    memcpy(output + position, digits + middle, DIGITS - middle);
    position += DIGITS - middle;

    memcpy(output + position, control, control_length);
    position += control_length;

    output[position++] = '\n';
    output[position] = '\0';

    if (fwrite(output, 1, position, stdout) != position) {
        fail("failed to write output");
    }

    fprintf(stderr, "control sequence: %s\n", control);
    fprintf(stderr, "control length: %zu decimal digits\n", control_length);
    fprintf(stderr, "random digits: %d\n", DIGITS);
    fprintf(stderr, "occurrences at start/middle/end: %zu\n",
            count_occurrences(output, control));
    fprintf(stderr, "total bytes written: %zu\n", position);

    free(output);
    free(digits);

    return 0;
}
