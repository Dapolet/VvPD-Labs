#include <stdio.h>
#include <string.h>

/*
 * Лабораторная работа № 3, п. 2.2.
 * UTF-8: одна кодовая точка может занимать от 1 до 4 байт.
 * strlen() возвращает число байт до '\0', а не число видимых символов.
 */

static void dump_bytes(const char *label, const char *text, size_t object_size)
{
    printf("=== %s ===\n", label);
    printf("text: %s\n", text);
    printf("strlen: %zu\n", strlen(text));
    printf("object size (sizeof): %zu\n", object_size);
    printf("bytes: ");

    for (size_t i = 0; i < strlen(text); ++i) {
        /* Приведение к unsigned char обязательно: иначе байты >= 0x80
         * могут печататься как отрицательные числа. */
        printf("%02X ", (unsigned char)text[i]);
    }

    printf("\n\n");
}

/*
 * Дополнительное задание: подсчёт кодовых точек в корректной UTF-8 строке
 * без Unicode-библиотеки. Продолжающие байты имеют вид 10xxxxxx (0x80..0xBF),
 * поэтому каждая кодовая точка начинается с байта, который не является
 * продолжающим.
 *
 * Ограничение: функция не валидирует UTF-8. Для обрезанной, переудлинённой
 * (overlong) или иным образом некорректной последовательности результат
 * не определён как число реальных символов.
 */
static size_t utf8_code_points(const char *text)
{
    size_t count = 0;
    const unsigned char *p = (const unsigned char *)text;

    while (*p != 0) {
        if ((*p & 0xC0) != 0x80) {
            ++count;
        }
        ++p;
    }

    return count;
}

int main(void)
{
    char hello[] = "Hello";
    char privet[] = "Привет";
    char yozh[] = "ёж";
    char daigaku[] = "大学";
    char spaces[] = "     ";

    dump_bytes("Hello (ASCII)", hello, sizeof(hello));
    dump_bytes("Привет (кириллица)", privet, sizeof(privet));
    dump_bytes("ёж (кириллица, 2 буквы)", yozh, sizeof(yozh));
    dump_bytes("大学 (CJK, 2 иероглифа)", daigaku, sizeof(daigaku));
    dump_bytes("Пять пробелов", spaces, sizeof(spaces));

    printf("=== Кодовые точки (дополнительно) ===\n");
    printf("%-10s %10s %10s\n", "text", "bytes", "code points");
    printf("%-10s %10zu %10zu\n", "Hello", strlen(hello), utf8_code_points(hello));
    printf("%-10s %10zu %10zu\n", "Привет", strlen(privet), utf8_code_points(privet));
    printf("%-10s %10zu %10zu\n", "ёж", strlen(yozh), utf8_code_points(yozh));
    printf("%-10s %10zu %10zu\n", "大学", strlen(daigaku), utf8_code_points(daigaku));
    printf("%-10s %10zu %10zu\n", "spaces", strlen(spaces), utf8_code_points(spaces));

    return 0;
}
