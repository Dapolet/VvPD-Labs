#include <stddef.h>
#include <stdio.h>

int main(void)
{
    int value = 304;
    int *ptr = &value;

    printf("=== значение, адрес, указатель, разыменование ===\n");
    printf("value         = %d\n", value);
    printf("&value        = %p\n", (void *)&value);
    printf("ptr           = %p\n", (void *)ptr);
    printf("*ptr          = %d\n", *ptr);

    *ptr = 305;
    printf("value after *ptr = 305: %d\n", value);

    printf("&ptr          = %p\n", (void *)&ptr);
    printf("sizeof(ptr)   = %zu байт\n", sizeof(ptr));
    printf("sizeof(value) = %zu байт\n", sizeof(value));

    int **pptr = &ptr;
    printf("\n=== указатель на указатель ===\n");
    printf("pptr          = %p\n", (void *)pptr);
    printf("*pptr         = %p\n", (void *)*pptr);
    printf("**pptr        = %d\n", **pptr);

    printf("\n=== размеры указателей разных типов ===\n");
    char *pc = NULL;
    int *pi = NULL;
    double *pd = NULL;
    void *pv = NULL;

    printf("sizeof(char *)   = %zu\n", sizeof(pc));
    printf("sizeof(int *)    = %zu\n", sizeof(pi));
    printf("sizeof(double *) = %zu\n", sizeof(pd));
    printf("sizeof(void *)   = %zu\n", sizeof(pv));

    printf("\n=== адреса самих переменных ===\n");
    printf("&ptr == &value: %s\n",
           (void *)&ptr == (void *)&value ? "yes" : "no");
    printf("&ptr != &value: %s\n",
           (void *)&ptr != (void *)&value ? "yes" : "no");
    printf("&ptr и &value — адреса разных объектов\n");

    return 0;
}
