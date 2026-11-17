# Лабораторная работа № 4

**Указатели, динамическая память, структуры, файлы и собственные типы данных**

Проект показывает цепочку

```text
адрес → указатель → динамическая память → структура → файл →
собственный тип → BigUInt → измерение производительности
```

Все эксперименты выполнены на macOS 15.8.1 (arm64, Apple M1 Pro).
Основной компилятор — Homebrew GCC 16.2.0 (`gcc-16`/`g++-16`);
дополнительно использован Apple Clang 16.0.0 для ASan/UBSan.

## Окружение

| Инструмент | Версия |
|---|---|
| ОС | macOS 15.8.1 (24H32), arm64 |
| Процессор | Apple M1 Pro |
| GCC | Homebrew GCC 16.2.0 (`gcc-16`) |
| G++ | Homebrew G++ 16.2.0 (`g++-16`) |
| Apple Clang | 16.0.0 (clang-1600.0.26.6) |
| GNU Make | 3.81 |
| Стандарт C | C17 (`-std=c17`) |
| Стандарт C++ | GNU++17 (`-std=gnu++17`) |
| Предупреждения | `-Wall -Wextra -Wpedantic` |

На macOS команда `gcc` обычно является Apple Clang, поэтому Makefile
автоматически выбирает `gcc-16`, если он есть в `PATH`.

## Структура

```text
VvPD-Lab4/
├── pointers/
│   ├── pointers.c          # значение, адрес, указатель, sizeof
│   ├── arrays.c            # массив, указатель, a[i] и *(a+i)
│   └── dynamic.c           # malloc/calloc/realloc/free, goto cleanup
├── structs_files/
│   ├── structs.c           # формы struct, typedef, . и ->, padding
│   ├── text_file.c         # текстовая запись и чтение
│   ├── binary_file.c       # fwrite/fread массива структур
│   └── pointer_trap.c      # указатель внутри структуры и hexdump
├── cpp_memory/
│   ├── new_delete.cpp      # new/delete, new[]/delete[], bad_alloc
│   └── raii.cpp            # vector, unique_ptr, RAII для FILE *
├── fixed/
│   ├── fixed.h
│   ├── fixed.c             # Money (масштаб 100) и Q16.16
│   └── main.c
├── linked_number/
│   ├── digit.h
│   ├── digit.c             # двусвязный список десятичных цифр
│   └── main.c
├── bigint/
│   ├── bigint.h
│   ├── bigint.c            # BigUInt на uint32_t limbs
│   ├── main.c              # тесты, 2^304/2^4096/2^16384, 100! и 1000!
│   └── benchmark.cpp       # uint16/uint32/uint64, -O0/-O2
├── artifacts/              # сохранённые выводы и результаты
├── Makefile
├── README.md
├── REPORT.md
└── VPD_04_2026.pdf
```

## Быстрый старт

```bash
make            # собрать все программы
make test       # запустить все демонстрации
make artifacts  # сохранить выводы в artifacts/
make bench-run  # бенчмарк -O0/-O2, 10 запусков
make sanitize   # ASan/UBSan для ключевых программ
make clean      # удалить исполняемые файлы
```

Примеры:

```bash
./pointers/pointers
./structs_files/pointer_trap artifacts/pointer_trap.dat
./fixed/fixed_demo
./bigint/bigint_demo
./bigint/benchmark_O2
```

## Что демонстрирует каждая часть

### Указатели

- `&value` и `ptr` содержат один и тот же адрес;
- `&ptr` — адрес самой переменной-указателя;
- `*ptr = 305` изменяет `value`;
- на arm64 все указатели в этом проекте имеют размер 8 байт;
- `sizeof(a)` для `int a[5]` равен 20, а `sizeof(&a[0])` — 8;
- `a[i]` и `*(a + i)` эквивалентны;
- `p + 1` переходит к следующему объекту типа `int`, а не на один байт.

### Динамическая память

- `malloc` не инициализирует память, `calloc` заполняет её нулевыми байтами;
- `sizeof *data` не повторяет имя типа вручную;
- `realloc` вызывается через временный указатель;
- ёмкость массива удваивается: при 10 элементах получается capacity 16;
- `goto cleanup` даёт одну точку освобождения `free`/`fclose`;
- разобраны memory leak, dangling pointer, use-after-free, double free,
  out-of-bounds, неинициализированный/NULL-указатель и потеря указателя
  при `realloc`.

### C++

- `new`/`delete` и `new[]`/`delete[]`;
- конструктор и деструктор вызываются автоматически;
- `malloc` не вызывает конструктор;
- `new` сообщает об ошибке через `std::bad_alloc`;
- `new (std::nothrow)` возвращает `nullptr`;
- `std::vector`, `std::unique_ptr` и собственный `FileHandle` реализуют RAII.

### Структуры и файлы

- шесть форм определения `struct` и `typedef`;
- инициализация по порядку и по именам полей (designated initializer);
- `p->age` эквивалентно `(*p).age`;
- `struct Example { char a; int b; char c; }` занимает 12 байт при сумме
  полей 6 байт; `offsetof` показывает позиции 0, 4, 8;
- текстовая запись/чтение пяти чисел, сумма 30;
- бинарная запись трёх `Student`: 3 × 72 = 216 байт, обратное чтение
  сохраняет поля;
- `fwrite` структуры с `char *name` записывает **числовое значение
  указателя**, а не строку; в hexdump видны байты адреса;
- структура с `char name[64]` сохраняет строку внутри себя и читается
  обратно корректно;
- бинарный снимок структуры не является переносимым форматом: мешают
  padding, alignment, endianness, размеры типов, версии структуры и
  указатели.

### Числа с фиксированной точкой

- `Money` хранит `int64_t raw` в сотых долях: `12345` = 123.45;
- `money_parse` точно разбирает `-12.34` и отвергает `12.345`;
- `money(0.1) + money(0.2)` даёт ровно 0.30, хотя `0.1 + 0.2` в `double`
  равно `0.30000000000000004`;
- Q16.16 хранит 16 целых и 16 дробных бит;
- `Q16(0.1) + Q16(0.2)` совпадает с `Q16(0.3)` (raw = 19661);
- граничные значения насыщаются: 32768 → `INT32_MAX`,
  −32769 → `INT32_MIN`.

### Связное десятичное число

- каждая цифра — отдельный узел `Digit` с `prev`/`next`;
- обход возможен слева направо и справа налево;
- `sizeof(Digit)` = 24 байта, то есть 24 байта на одну цифру;
- для 51 цифры занято 1224 байта вместо 51 байта полезной нагрузки;
- все узлы освобождаются, повторный `decimal_free` безопасен.

### BigUInt

- `limbs[0]` — младшие 32 бита, `size` — число блоков, `capacity` — ёмкость;
- ноль представлен как `size == 0`;
- реализованы `init`, `free`, `reserve`, `normalize`, `from_u64`, `from_hex`,
  `copy`, `compare`, `add`, `sub`, `mul`, `mul_u64`, `pow2`, `to_u64`,
  `bit_length`, `print_hex`, `print_dec`;
- сложение использует `uint64_t` для суммы двух 32-битных блоков и переноса;
- вычитание требует `a >= b` и обрабатывает заём между блоками;
- умножение — школьное O(n²);
- встроенные тесты покрывают 0+0, 1+1, `0xFFFFFFFF + 1`,
  `0xFFFFFFFFFFFFFFFF + 1`, разные размеры, ноль, alias и рост capacity;
- `2^304`, `2^4096`, `2^16384` выводятся в hex;
- дополнительное задание: `100!` и `1000!` (десятичная запись).

### Бенчмарк

Сравниваются `uint16_t`, `uint32_t`, `uint64_t` на 4096-битных операндах,
операции сложения и удвоения, 10 запусков, 200 000 итераций на запуск.
Собираются варианты `-O0` и `-O2`; для каждого варианта выводятся
минимум, максимум, среднее и медиана в наносекундах на операцию.

Полные результаты: `artifacts/benchmark.txt`.

## Ключевые артефакты

| Файл | Содержимое |
|---|---|
| `artifacts/run_pointers.txt` | адреса, размеры указателей |
| `artifacts/run_arrays.txt` | массив и арифметика указателей |
| `artifacts/run_dynamic.txt` | malloc/calloc/realloc, goto cleanup |
| `artifacts/run_structs.txt` | формы struct, padding, offsetof |
| `artifacts/run_text_file.txt` | текстовая запись/чтение, сумма 30 |
| `artifacts/run_binary_file.txt` | fwrite/fread, 216 байт, 3 записи |
| `artifacts/run_pointer_trap.txt` | hexdump и указатель в файле |
| `artifacts/run_new_delete.txt` | new/delete, конструктор, bad_alloc |
| `artifacts/run_raii.txt` | vector, unique_ptr, FileHandle |
| `artifacts/run_fixed.txt` | Money и Q16.16 |
| `artifacts/run_linked_number.txt` | двусвязные цифры, sizeof(Digit) |
| `artifacts/run_bigint.txt` | тесты BigUInt, степени двойки, факториалы |
| `artifacts/benchmark.txt` | результаты -O0/-O2 |
| `artifacts/asan_*.txt` | прогоны под ASan/UBSan |

## Воспроизводимость

```bash
make clean
make
make test
make artifacts
make bench-run
make sanitize
```

Бенчмарк занимает около 20 секунд на Apple M1 Pro. Все команды
используют `gcc-16`/`g++-16`, если они установлены; иначе — `cc`/`c++`.
