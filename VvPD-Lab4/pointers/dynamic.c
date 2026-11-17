#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int *data;
    size_t size;
    size_t capacity;
} IntVector;

static bool vector_reserve(IntVector *vector, size_t needed)
{
    if (needed <= vector->capacity)
        return true;

    size_t new_capacity = vector->capacity ? vector->capacity : 4;
    while (new_capacity < needed) {
        if (new_capacity > (size_t)-1 / 2)
            return false;
        new_capacity *= 2;
    }

    int *temporary = realloc(vector->data, new_capacity * sizeof *temporary);
    if (temporary == NULL)
        return false;

    vector->data = temporary;
    vector->capacity = new_capacity;
    return true;
}

static bool vector_push(IntVector *vector, int value)
{
    if (!vector_reserve(vector, vector->size + 1))
        return false;

    vector->data[vector->size++] = value;
    return true;
}

static void vector_free(IntVector *vector)
{
    free(vector->data);
    vector->data = NULL;
    vector->size = 0;
    vector->capacity = 0;
}

static int read_sum(const char *path, size_t limit)
{
    int result = 1;
    FILE *file = NULL;
    int *buffer = NULL;

    file = fopen(path, "r");
    if (file == NULL) {
        perror(path);
        goto cleanup;
    }

    buffer = malloc(limit * sizeof *buffer);
    if (buffer == NULL) {
        fputs("malloc failed\n", stderr);
        goto cleanup;
    }

    size_t count = 0;
    long sum = 0;
    while (count < limit && fscanf(file, "%d", &buffer[count]) == 1) {
        sum += buffer[count];
        ++count;
    }

    printf("process_file: read %zu values, sum = %ld\n", count, sum);
    result = 0;

cleanup:
    free(buffer);
    if (file != NULL)
        fclose(file);
    return result;
}

int main(int argc, char **argv)
{
    size_t n = 10;
    if (argc > 1) {
        char *end = NULL;
        unsigned long parsed = strtoul(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || parsed == 0) {
            fprintf(stderr, "usage: %s [positive N]\n", argv[0]);
            return 1;
        }
        n = (size_t)parsed;
    }

    printf("=== растущий массив: malloc/realloc/free ===\n");
    IntVector vector = {NULL, 0, 0};
    for (size_t i = 0; i < n; ++i) {
        if (!vector_push(&vector, (int)(i * i))) {
            fputs("allocation failed\n", stderr);
            vector_free(&vector);
            return 1;
        }
    }

    printf("size = %zu, capacity = %zu\n", vector.size, vector.capacity);
    printf("data =");
    for (size_t i = 0; i < vector.size; ++i)
        printf(" %d", vector.data[i]);
    printf("\n");

    printf("\n=== malloc и calloc: разные гарантии ===\n");
    const size_t block_size = 5;
    int *raw = malloc(block_size * sizeof *raw);
    int *zeroed = calloc(block_size, sizeof *zeroed);

    if (raw == NULL || zeroed == NULL) {
        fputs("allocation failed\n", stderr);
        free(raw);
        free(zeroed);
        vector_free(&vector);
        return 1;
    }

    printf("malloc block at %p: содержимое не читаем (значения не определены)\n",
           (void *)raw);
    bool all_zero = true;
    for (size_t i = 0; i < block_size; ++i) {
        if (zeroed[i] != 0)
            all_zero = false;
    }
    printf("calloc block at %p: все %zu значений равны нулю: %s\n",
           (void *)zeroed, block_size, all_zero ? "yes" : "no");

    free(raw);
    free(zeroed);

    printf("\n=== goto cleanup: единая точка освобождения ===\n");
    FILE *temporary = fopen("dynamic_input.txt", "w");
    if (temporary == NULL) {
        perror("dynamic_input.txt");
        vector_free(&vector);
        return 1;
    }
    for (int i = 0; i < 5; ++i)
        fprintf(temporary, "%d\n", i * i);
    fclose(temporary);

    int success = read_sum("dynamic_input.txt", 5);
    printf("read_sum(existing) = %d\n", success);
    int missing = read_sum("definitely_missing_file.txt", 5);
    printf("read_sum(missing)  = %d (очистка всё равно выполнена)\n", missing);
    remove("dynamic_input.txt");

    printf("\n=== sizeof *data вместо sizeof(int) ===\n");
    int *example = NULL;
    printf("sizeof *example = %zu, sizeof(int) = %zu\n",
           sizeof *example, sizeof(int));

    vector_free(&vector);
    printf("vector freed: data=%p, size=%zu, capacity=%zu\n",
           (void *)vector.data, vector.size, vector.capacity);

    return 0;
}
