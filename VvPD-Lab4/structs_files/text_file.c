#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "numbers.txt";

    printf("=== текстовая запись ===\n");
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        perror(path);
        return 1;
    }

    for (int i = 0; i < 5; ++i) {
        if (fprintf(file, "%d\n", i * i) < 0) {
            perror("fprintf");
            fclose(file);
            return 1;
        }
    }

    if (fclose(file) != 0) {
        perror("fclose");
        return 1;
    }
    printf("записано 5 строк в %s\n", path);

    printf("\n=== текстовая запись закрыта, открываем заново ===\n");
    file = fopen(path, "r");
    if (file == NULL) {
        perror(path);
        return 1;
    }

    char line[128];
    long sum = 0;
    int count = 0;

    while (fgets(line, sizeof line, file) != NULL) {
        printf("line: %s", line);

        char *end = NULL;
        long value = strtol(line, &end, 10);
        if (end != line) {
            sum += value;
            ++count;
        }
    }

    if (ferror(file)) {
        perror("fgets");
        fclose(file);
        return 1;
    }

    if (fclose(file) != 0) {
        perror("fclose");
        return 1;
    }

    printf("\nread %d numbers, sum = %ld\n", count, sum);
    printf("expected sum 0+1+4+9+16 = 30\n");

    return 0;
}
