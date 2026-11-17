#include <stdio.h>

int main(void)
{
    int a[5] = {10, 20, 30, 40, 50};

    printf("=== массив и указатель ===\n");
    printf("sizeof(a)       = %zu байт\n", sizeof(a));
    printf("sizeof(a[0])    = %zu байт\n", sizeof(a[0]));
    printf("sizeof(&a[0])   = %zu байт\n", sizeof(&a[0]));
    printf("sizeof(&a)      = %zu байт\n", sizeof(&a));
    printf("number of items = %zu\n", sizeof(a) / sizeof(a[0]));

    printf("\n=== a[i] и *(a + i) ===\n");
    for (size_t i = 0; i < 5; ++i) {
        printf("i=%zu  a[i]=%d  *(a+i)=%d  &a[i]=%p  a+i=%p\n",
               i, a[i], *(a + i), (void *)&a[i], (void *)(a + i));
    }

    printf("\n=== арифметика указателей ===\n");
    for (int *p = a; p < a + 5; ++p) {
        printf("address=%p  value=%d\n", (void *)p, *p);
    }

    int *first = &a[0];
    int *second = &a[1];
    printf("second - first = %td (в элементах int)\n", second - first);
    printf("(char *)second - (char *)first = %td байт\n",
           (char *)second - (char *)first);
    printf("sizeof(int) = %zu\n", sizeof(int));

    printf("\n=== &a и &a + 1 ===\n");
    printf("&a     = %p\n", (void *)&a);
    printf("&a + 1 = %p\n", (void *)(&a + 1));
    printf("(char *)(&a + 1) - (char *)&a = %td байт\n",
           (char *)(&a + 1) - (char *)&a);

    return 0;
}
