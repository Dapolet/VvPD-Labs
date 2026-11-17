#include <stdio.h>
#include <string.h>

/*
 * Лабораторная работа № 3, п. 2.1.
 * Базовый эксперимент с ASCII: sizeof() против strlen().
 */
int main(void)
{
    char text[] = "Hello";

    printf("Text: %s\n", text);
    printf("sizeof(text): %zu bytes (object size, includes the terminating '\\0')\n", sizeof(text));
    printf("strlen(text): %zu bytes (characters before the terminating '\\0')\n", strlen(text));
    printf("\n");
    printf("index  char   dec   hex\n");

    for (size_t i = 0; i < sizeof(text); ++i) {
        unsigned char byte = (unsigned char)text[i];

        if (byte == 0) {
            printf("%5zu  '\\0'  %3u   0x%02X  <- terminator, not counted by strlen\n",
                   i, byte, byte);
        } else {
            printf("%5zu   '%c'  %3u   0x%02X\n", i, byte, byte, byte);
        }
    }

    return 0;
}
