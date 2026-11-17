#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t id;
    char *name;
} StudentPtr;

typedef struct {
    uint32_t id;
    char name[64];
} StudentFixed;

static void hexdump(const unsigned char *data, size_t size)
{
    for (size_t offset = 0; offset < size; offset += 16) {
        printf("%08zx  ", offset);

        for (size_t i = 0; i < 16; ++i) {
            if (offset + i < size)
                printf("%02x ", data[offset + i]);
            else
                printf("   ");
        }

        printf(" |");
        for (size_t i = 0; i < 16 && offset + i < size; ++i) {
            unsigned char c = data[offset + i];
            putchar(c >= 32 && c <= 126 ? c : '.');
        }
        printf("|\n");
    }
}

static int demonstrate_pointer_field(const char *path)
{
    StudentPtr student = {1, "Ivan Ivanov"};

    printf("=== структура с полем char * ===\n");
    printf("sizeof(StudentPtr) = %zu\n", sizeof(StudentPtr));
    printf("offsetof(name) = %zu\n", offsetof(StudentPtr, name));
    printf("student.name = %p, string = \"%s\"\n",
           (void *)student.name, student.name);

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        perror(path);
        return 1;
    }

    size_t written = fwrite(&student, sizeof student, 1, file);
    if (written != 1) {
        fputs("fwrite failed\n", stderr);
        fclose(file);
        return 1;
    }

    if (fclose(file) != 0) {
        perror("fclose");
        return 1;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        return 1;
    }

    unsigned char *buffer = malloc(sizeof student);
    if (buffer == NULL) {
        fputs("malloc failed\n", stderr);
        fclose(file);
        return 1;
    }

    size_t got = fread(buffer, 1, sizeof student, file);
    fclose(file);

    printf("file contains %zu bytes; dump:\n", got);
    hexdump(buffer, got);

    uintptr_t stored = 0;
    memcpy(&stored, buffer + offsetof(StudentPtr, name), sizeof stored);
    printf("pointer bits in file = 0x%016llx\n",
           (unsigned long long)stored);
    printf("original pointer    = %p\n", (void *)student.name);
    printf("numeric value is equal in this process: %s\n",
           stored == (uintptr_t)(void *)student.name ? "yes" : "no");
    printf("the string bytes themselves are not stored by fwrite\n");

    free(buffer);
    return 0;
}

static int demonstrate_fixed_field(const char *path)
{
    StudentFixed student = {2, "Anna Petrova"};

    printf("\n=== структура с массивом char[64] ===\n");
    printf("sizeof(StudentFixed) = %zu\n", sizeof(StudentFixed));
    printf("offsetof(name) = %zu\n", offsetof(StudentFixed, name));

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        perror(path);
        return 1;
    }

    if (fwrite(&student, sizeof student, 1, file) != 1) {
        fputs("fwrite failed\n", stderr);
        fclose(file);
        return 1;
    }
    fclose(file);

    StudentFixed loaded;
    file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        return 1;
    }

    size_t got = fread(&loaded, sizeof loaded, 1, file);
    fclose(file);

    if (got != 1) {
        fputs("fread failed\n", stderr);
        return 1;
    }

    printf("loaded.id = %u, loaded.name = \"%s\"\n", loaded.id, loaded.name);
    printf("string is stored inside the structure and survives the file\n");

    return strcmp(student.name, loaded.name) == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "pointer_trap.dat";

    if (demonstrate_pointer_field(path) != 0)
        return 1;

    if (demonstrate_fixed_field(path) != 0)
        return 1;

    return 0;
}
