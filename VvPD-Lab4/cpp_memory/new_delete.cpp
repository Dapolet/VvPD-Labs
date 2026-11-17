#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

struct Student {
    Student()
    {
        std::cout << "Student::Student()\n";
    }

    ~Student()
    {
        std::cout << "Student::~Student()\n";
    }

    int id = 304;
};

int main()
{
    std::cout << "=== new/delete для одного объекта ===\n";
    int *value = new int(304);
    std::cout << "*value = " << *value << '\n';
    delete value;
    value = nullptr;
    std::cout << "value after delete = "
              << (value == nullptr ? "nullptr" : "not null") << '\n';

    std::cout << "\n=== new[]/delete[] ===\n";
    int *data = new int[5]{};
    for (int i = 0; i < 5; ++i)
        data[i] = i * i;

    long sum = 0;
    for (int i = 0; i < 5; ++i)
        sum += data[i];
    std::cout << "sum of 0,1,4,9,16 = " << sum << '\n';
    delete[] data;

    std::cout << "\n=== конструктор и деструктор ===\n";
    {
        Student *student = new Student;
        std::cout << "student->id = " << student->id << '\n';
        delete student;
    }

    std::cout << "\n=== malloc не вызывает конструктор ===\n";
    {
        void *raw = std::malloc(sizeof(Student));
        std::cout << "malloc returned " << raw
                  << " (raw memory, no constructor)\n";
        std::free(raw);
    }

    std::cout << "\n=== std::bad_alloc ===\n";
    try {
        /*
         * Обычная форма new[] при невозможности выделить память сообщает
         * об ошибке исключением std::bad_alloc. Здесь вызывается нижний
         * operator new с заведомо невозможным размером, чтобы не просить
         * у системы реальный гигантский блок.
         */
        void *huge = ::operator new(std::numeric_limits<std::size_t>::max());
        ::operator delete(huge);
        std::cout << "unexpected: huge allocation succeeded\n";
    }
    catch (const std::bad_alloc &error) {
        std::cout << "allocation failed: " << error.what() << '\n';
    }

    std::cout << "\n=== std::nothrow ===\n";
    int *other = new (std::nothrow) int[1000];
    if (other == nullptr) {
        std::cout << "nothrow allocation failed\n";
    }
    else {
        std::cout << "nothrow allocation succeeded\n";
        delete[] other;
    }

    std::cout << "\nsizeof(Student) = " << sizeof(Student) << '\n';
    return 0;
}
