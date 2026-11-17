#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t id;
    char name[64];
    uint32_t group_id;
} Student;

static int same_student(const Student *left, const Student *right)
{
    return left->id == right->id &&
           left->group_id == right->group_id &&
           strcmp(left->name, right->name) == 0;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "students.dat";

    Student students[] = {
        {1, "Ivan Ivanov", 304},
        {2, "Anna Petrova", 304},
        {3, "Petr Sidorov", 305}
    };
    const size_t count = sizeof students / sizeof students[0];

    printf("=== бинарная запись массива структур ===\n");
    printf("sizeof(Student) = %zu, records = %zu\n", sizeof(Student), count);

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        perror(path);
        return 1;
    }

    size_t written = fwrite(students, sizeof students[0], count, file);
    if (written != count) {
        fputs("fwrite wrote fewer records than expected\n", stderr);
        fclose(file);
        return 1;
    }

    if (fclose(file) != 0) {
        perror("fclose");
        return 1;
    }
    printf("fwrite: written = %zu\n", written);

    file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        return 1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(file);
        return 1;
    }
    long file_size = ftell(file);
    rewind(file);
    printf("file size = %ld bytes, expected = %zu bytes\n",
           file_size, count * sizeof(Student));

    Student loaded[10];
    size_t got = fread(loaded, sizeof loaded[0], 10, file);
    if (ferror(file)) {
        perror("fread");
        fclose(file);
        return 1;
    }

    if (fclose(file) != 0) {
        perror("fclose");
        return 1;
    }

    printf("fread: got = %zu\n", got);
    for (size_t i = 0; i < got; ++i) {
        printf("  %u | %-16s | %u\n",
               loaded[i].id, loaded[i].name, loaded[i].group_id);
    }

    int equal = got == count;
    for (size_t i = 0; equal && i < count; ++i)
        equal = same_student(&students[i], &loaded[i]);

    printf("round-trip fields match: %s\n", equal ? "yes" : "no");

    return equal ? 0 : 1;
}
