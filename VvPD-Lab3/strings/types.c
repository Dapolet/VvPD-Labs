#include <limits.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/*
 * Лабораторная работа № 3, пп. 2.3–2.5.
 *   2.3. Одинаковые ASCII-коды в массивах разных целочисленных типов.
 *   2.4. wchar_t и wide strings.
 *   2.5. <stdint.h>: exact-width, least и fast типы.
 */

static void print_chars_from_char(void)
{
    char a[] = {'H', 'e', 'l', 'l', 'o'};

    for (size_t i = 0; i < sizeof(a) / sizeof(a[0]); ++i) {
        putchar((unsigned char)a[i]);
    }
    putchar('\n');
}

static void print_chars_from_short(void)
{
    short b[] = {'H', 'e', 'l', 'l', 'o'};

    for (size_t i = 0; i < sizeof(b) / sizeof(b[0]); ++i) {
        putchar((unsigned char)b[i]);
    }
    putchar('\n');
}

static void print_chars_from_int(void)
{
    int c[] = {'H', 'e', 'l', 'l', 'o'};

    for (size_t i = 0; i < sizeof(c) / sizeof(c[0]); ++i) {
        putchar((unsigned char)c[i]);
    }
    putchar('\n');
}

static void print_chars_from_ull(void)
{
    unsigned long long d[] = {'H', 'e', 'l', 'l', 'o'};

    for (size_t i = 0; i < sizeof(d) / sizeof(d[0]); ++i) {
        putchar((unsigned char)d[i]);
    }
    putchar('\n');
}

static void setup_locale(void)
{
    static const char *const fallbacks[] = {"C.UTF-8", "en_US.UTF-8", "UTF-8"};

    /* На macOS переменная LANG может быть не задана, и setlocale("") даёт "C",
     * в котором printf("%ls") не выведет кириллицу. Поэтому проверяем
     * результат и при необходимости пробуем явные UTF-8 локали. */
    if (setlocale(LC_ALL, "") != NULL) {
        const char *current = setlocale(LC_CTYPE, NULL);

        if (current != NULL && strcmp(current, "C") != 0 && strcmp(current, "POSIX") != 0) {
            return;
        }
    }

    for (size_t i = 0; i < sizeof(fallbacks) / sizeof(fallbacks[0]); ++i) {
        if (setlocale(LC_ALL, fallbacks[i]) != NULL) {
            return;
        }
    }
}

int main(void)
{
    setup_locale();

    puts("=== 2.3. Одинаковые коды — разные типы элементов ===");
    print_chars_from_char();
    print_chars_from_short();
    print_chars_from_int();
    print_chars_from_ull();

    char a[] = {'H', 'e', 'l', 'l', 'o'};
    short b[] = {'H', 'e', 'l', 'l', 'o'};
    int c[] = {'H', 'e', 'l', 'l', 'o'};
    unsigned long long d[] = {'H', 'e', 'l', 'l', 'o'};

    printf("char[]               : %zu bytes\n", sizeof(a));
    printf("short[]              : %zu bytes\n", sizeof(b));
    printf("int[]                : %zu bytes\n", sizeof(c));
    printf("unsigned long long[] : %zu bytes\n", sizeof(d));

    puts("\n=== 2.4. wchar_t и wide strings ===");
    wchar_t wide_ascii[] = L"Hello";
    wchar_t wide_cyrillic[] = L"Привет";

    printf("sizeof(wchar_t)          = %zu bytes\n", sizeof(wchar_t));
    printf("sizeof(wide_ascii)       = %zu bytes\n", sizeof(wide_ascii));
    printf("wcslen(wide_ascii)       = %zu code units\n", wcslen(wide_ascii));
    printf("wide_ascii text          = %ls\n", wide_ascii);
    printf("sizeof(wide_cyrillic)    = %zu bytes\n", sizeof(wide_cyrillic));
    printf("wcslen(wide_cyrillic)    = %zu code units\n", wcslen(wide_cyrillic));
    printf("wide_cyrillic text       = %ls\n", wide_cyrillic);
    printf("wchar_t code units (hex): ");
    for (size_t i = 0; i < wcslen(wide_cyrillic); ++i) {
        printf("%04X ", (unsigned int)wide_cyrillic[i]);
    }
    printf("\n");

    puts("\n=== 2.5. <stdint.h> ===");
    printf("CHAR_BIT          = %d\n", CHAR_BIT);
    printf("sizeof(short)     = %zu bytes\n", sizeof(short));
    printf("sizeof(int)       = %zu bytes\n", sizeof(int));
    printf("sizeof(long)      = %zu bytes\n", sizeof(long));
    printf("sizeof(long long) = %zu bytes\n", sizeof(long long));

#ifdef UINT8_MAX
    printf("uint8_t           = %zu bytes, %zu bits\n",
           sizeof(uint8_t), sizeof(uint8_t) * CHAR_BIT);
#endif
#ifdef UINT16_MAX
    printf("uint16_t          = %zu bytes, %zu bits\n",
           sizeof(uint16_t), sizeof(uint16_t) * CHAR_BIT);
#endif
#ifdef UINT32_MAX
    printf("uint32_t          = %zu bytes, %zu bits\n",
           sizeof(uint32_t), sizeof(uint32_t) * CHAR_BIT);
#endif
#ifdef UINT64_MAX
    printf("uint64_t          = %zu bytes, %zu bits\n",
           sizeof(uint64_t), sizeof(uint64_t) * CHAR_BIT);
#endif

    printf("uint_least32_t    = %zu bytes\n", sizeof(uint_least32_t));
    printf("uint_fast32_t     = %zu bytes\n", sizeof(uint_fast32_t));

    return 0;
}
