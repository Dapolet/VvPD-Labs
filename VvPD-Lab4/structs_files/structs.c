#include <stddef.h>
#include <stdio.h>

/* 1. Предварительное объявление. */
struct Person;

/* 2. Обычная именованная структура. */
struct Person {
    char name[64];
    int age;
};

/* 3. Определение структуры и переменной одновременно. */
struct Employee {
    char name[64];
    int age;
} tom_employee = {"Tom", 38};

/* 4. Анонимная структура с переменной. */
struct {
    char name[64];
    int age;
} anonymous_tom = {"Anonymous", 20};

/* 5. Анонимная структура с псевдонимом typedef. */
typedef struct {
    char name[64];
    int age;
} PersonT;

/* 6. Именованная структура и псевдоним typedef. */
typedef struct NamedPerson {
    char name[64];
    int age;
} NamedPerson;

struct Example {
    char a;
    int b;
    char c;
};

struct Compact {
    int b;
    char a;
    char c;
};

static void print_person(const struct Person *person)
{
    printf("%s %d\n", person->name, person->age);
}

int main(void)
{
    struct Person a = {"Tom", 38};
    struct Person b = {
        .name = "Bob",
        .age = 41
    };
    struct Person *p = &a;

    printf("=== инициализация и операторы . и -> ===\n");
    printf("a: %s %d\n", a.name, a.age);
    printf("b: %s %d\n", b.name, b.age);
    printf("p->name = %s, p->age = %d\n", p->name, p->age);
    printf("(*p).age = %d\n", (*p).age);
    printf("p->age and (*p).age denote the same field: %d == %d\n",
           p->age, (*p).age);
    printf("p == &a: %s\n", (void *)p == (void *)&a ? "yes" : "no");
    print_person(p);

    printf("\n=== остальные формы struct ===\n");
    printf("employee: %s %d\n", tom_employee.name, tom_employee.age);
    printf("anonymous: %s %d\n", anonymous_tom.name, anonymous_tom.age);

    PersonT carol = {"Carol", 25};
    NamedPerson dave = {.name = "Dave", .age = 30};
    printf("PersonT: %s %d\n", carol.name, carol.age);
    printf("NamedPerson: %s %d\n", dave.name, dave.age);

    printf("\n=== размеры ===\n");
    printf("sizeof(struct Person) = %zu\n", sizeof(struct Person));
    printf("sizeof(struct Employee) = %zu\n", sizeof(struct Employee));
    printf("sizeof(PersonT) = %zu\n", sizeof(PersonT));
    printf("sizeof(NamedPerson) = %zu\n", sizeof(NamedPerson));

    printf("\n=== заполнение и выравнивание ===\n");
    struct Example example = {'A', 304, 'Z'};
    printf("sizeof(struct Example) = %zu\n", sizeof(struct Example));
    printf("sum of field sizes = %zu\n",
           sizeof(example.a) + sizeof(example.b) + sizeof(example.c));
    printf("offsetof(a) = %zu\n", offsetof(struct Example, a));
    printf("offsetof(b) = %zu\n", offsetof(struct Example, b));
    printf("offsetof(c) = %zu\n", offsetof(struct Example, c));

    struct Compact compact = {304, 'A', 'Z'};
    printf("\nsizeof(struct Compact) = %zu\n", sizeof(struct Compact));
    printf("offsetof(b) = %zu\n", offsetof(struct Compact, b));
    printf("offsetof(a) = %zu\n", offsetof(struct Compact, a));
    printf("offsetof(c) = %zu\n", offsetof(struct Compact, c));
    printf("compact: b=%d a=%c c=%c\n", compact.b, compact.a, compact.c);

    printf("\n=== указатель внутри структуры ===\n");
    printf("sizeof(char[64]) = %zu, sizeof(char *) = %zu\n",
           sizeof a.name, sizeof(char *));
    printf("sizeof(struct Person) = %zu, sizeof(name) + sizeof(age) = %zu\n",
           sizeof(struct Person), sizeof(a.name) + sizeof(a.age));

    return 0;
}
