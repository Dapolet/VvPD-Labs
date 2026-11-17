/*
 * Демонстрация двух стандартных форм точки входа C.
 *
 * Обычная сборка:
 *     cc argv_demo.c -o argv_demo
 *     ./argv_demo alpha "beta gamma"
 *
 * Сборка формы без параметров:
 *     cc -DNO_ARGS argv_demo.c -o argv_void_demo
 *     ./argv_void_demo
 */

#include <stdio.h>

#ifdef NO_ARGS

int main(void)
{
    printf("int main(void): no command-line parameters\n");
    printf("This form is standard C and is used when argc/argv are not needed.\n");
    return 0;
}

#else

int main(int argc, char *argv[])
{
    printf("int main(int argc, char *argv[]):\n");
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; ++i) {
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
    }
    return 0;
}

#endif
