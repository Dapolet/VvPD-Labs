# Отчёт по лабораторной работе № 5

**Тема:** Командная строка, файловая база данных и собственный язык запросов  
**Студент:** Полетаев Дмитрий, группа М3О-119БВ-26, номер 15  
**Проект:** `VvPD-Lab5`

---

## 1. Окружение

| Параметр | Значение |
|---|---|
| ОС | macOS 15.8.1 (24H32), arm64 |
| Процессор | Apple M1 Pro |
| Компилятор C | Homebrew GCC 16.2.0 (`gcc-16`) |
| Apple Clang | 16.0.0 (clang-1600.0.26.6) |
| GNU Make | 3.81 |
| Стандарт | C17 (`-std=c17`) |
| Предупреждения | `-Wall -Wextra -Wpedantic` |
| Санитайзеры | ASan + UBSan, Apple Clang 16 |

Проверка версий:

```text
gcc-16 (Homebrew GCC 16.2.0) 16.2.0
Apple clang version 16.0.0 (clang-1600.0.26.6)
GNU Make 3.81
macOS 15.8.1, Apple M1 Pro, arm64
```

В Makefile используется `gcc-16`, если он найден в `PATH`; иначе `cc`.
Все программы собираются без предупреждений с указанными флагами.
Дополнительно `database/vpddb`, `json/json_demo` и `filetool/filetool`
собраны и прогнаны под ASan/UBSan (`make sanitize`).

---

## 2. Структура проекта и сборка

```text
VvPD-Lab5/
├── filetool/
│   ├── filetool.c
│   ├── argv_demo.c
│   └── streams_demo.sh
├── database/
│   ├── model.h
│   ├── model.c
│   ├── storage.h
│   ├── storage.c
│   ├── database.h
│   ├── database.c
│   ├── config.h
│   ├── config.c
│   └── main.c
├── json/
│   ├── statham.json
│   ├── export.json
│   ├── json.h
│   ├── json.c
│   ├── database_json.h
│   ├── database_json.c
│   └── json_demo.c
├── query/
│   ├── query.h
│   ├── lexer.c
│   ├── parser.c
│   └── executor.c
├── tests/
│   ├── run_tests.sh
│   ├── make_artifacts.sh
│   ├── demo_session.txt
│   ├── test_cases.txt
│   └── sample_input.txt
├── artifacts/
├── config.yaml
├── Makefile
├── README.md
├── REPORT.md
└── VPD_05_2026.pdf
```

Сборка и запуск:

```bash
make            # filetool, argv-демо, vpddb, json_demo
make test       # 100 автоматических проверок
make artifacts  # выводы в artifacts/
make sanitize   # ASan/UBSan
make clean
```

---

## 3. Часть I. `main()`, `argc`, `argv` и коды завершения

### 3.1. Две стандартные формы `main`

```c
int main(void)
{
    /* programme without command-line parameters */
}

int main(int argc, char *argv[])
{
    /* programme receives command-line parameters */
}
```

`int main(void)` явно сообщает, что параметров нет. Форма
`int main(int argc, char *argv[])` используется, когда программа должна
получить аргументы запуска. Запись `void main()` не является стандартной
формой точки входа в hosted environment.

В проекте обе формы собраны из одного файла `filetool/argv_demo.c`:

```bash
./filetool/argv_demo alpha "beta gamma"
./filetool/argv_void_demo
```

Фактический вывод:

```text
$ ./filetool/argv_demo alpha "beta gamma"
int main(int argc, char *argv[]):
argc = 3
argv[0] = "./filetool/argv_demo"
argv[1] = "alpha"
argv[2] = "beta gamma"

$ ./filetool/argv_void_demo
int main(void): no command-line parameters
This form is standard C and is used when argc/argv are not needed.
```

Второй бинарник собирается из того же исходника с `-DNO_ARGS`:

```make
filetool/argv_void_demo: filetool/argv_demo.c
	$(CC) $(CFLAGS) -DNO_ARGS $< -o $@
```

### 3.2. Что находится в `argc` и `argv`

Для запуска

```text
filetool --input-file source.txt --output-file copy.txt --show-stats
```

концептуально получаем:

```text
argc = 6
argv[0] = "filetool"
argv[1] = "--input-file"
argv[2] = "source.txt"
argv[3] = "--output-file"
argv[4] = "copy.txt"
argv[5] = "--show-stats"
```

`argc` — количество элементов, `argv` — массив строк. Имя запускаемой
программы обычно находится в `argv[0]`. Оболочка разбивает командную
строку на аргументы по пробелам; кавычки позволяют передать аргумент
с пробелом (`"beta gamma"`) как один элемент.

Дополнительно `vpddb --show-args` печатает:

```text
argc = 2
argv[0] = "./database/vpddb"
argv[1] = "--show-args"
```

### 3.3. Коды завершения

```c
return 0;   /* success */
return 64;  /* command-line usage error */
```

Если выполнение `main` дошло до закрывающей фигурной скобки, это
эквивалентно `return 0`. Это специальное правило именно `main`, а не
разрешение забывать `return` в других функциях, возвращающих значение.
Любое ненулевое значение говорит об ошибке, если не описано обратное.

В проекте используются разные ненулевые коды для разных классов ошибок:

| Код | Класс | Где используется |
|---|---|---|
| `0` | успех | все программы |
| `64` | ошибка командной строки | неизвестный параметр, нет значения |
| `65` | ошибка запроса/JSON-синтаксиса | lexer/parser, повреждённый JSON |
| `66` | входной файл | нет файла базы, query-файла, JSON |
| `73` | выходной файл | нельзя открыть/записать БД или JSON |
| `74` | нарушение целостности/данных | дубликат ключа, плохой FK, ссылка |
| `75` | повреждённое хранилище | bad magic, checksum, размер |

Это позволяет вызывающей среде (скрипту, Makefile, CI) различать
«неправильно вызвали», «не нашли файл», «нарушили данные» и
«файл повреждён».

### 3.4. Контрольный эксперимент с неизвестным параметром

```bash
./filetool/filetool --vot-takogo-vida
```

Вывод:

```text
Error: unknown option: --vot-takogo-vida
Usage: filetool --input-file <file> --output-file <file> [--show-stats]
...
```

Код завершения — `64`. Операционная система сама по себе не знает смысла
строки `--vot-takogo-vida`: она лишь передаёт её в `argv`. Смысл параметру
придаёт код программы, который сравнивает `argv[i]` со строками
`--input-file`, `--output-file`, `--show-stats`. Поэтому неизвестный
параметр — это ошибка приложения, а не ошибка ОС.

---

## 4. Часть II. Утилита `filetool`

### 4.1. Параметры и соответствие kebab-case → snake_case

| CLI (kebab-case) | C (snake_case) |
|---|---|
| `--input-file` | `input_file` |
| `--output-file` | `output_file` |
| `--show-stats` | `show_stats` |

Разбор параметров:

```c
for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--input-file") == 0) {
        if (i + 1 >= argc) {
            fprintf(stderr, "Error: --input-file requires a file name\n");
            return 64;
        }
        input_file = argv[++i];
    } else if (strcmp(argv[i], "--output-file") == 0) {
        ...
    } else if (strcmp(argv[i], "--show-stats") == 0) {
        show_stats = 1;
    } else {
        fprintf(stderr, "Error: unknown option: %s\n", argv[i]);
        return 64;
    }
}
```

Если обязательный параметр отсутствует, программа печатает `Usage` в
`stderr` и возвращает `64`. Если не открывается входной файл — `66`,
выходной — `73`.

### 4.2. Копирование

Копирование выполняется блоками по 4096 байт через `fread`/`fwrite`:

```c
unsigned char buffer[4096];
unsigned long long bytes = 0;

for (;;) {
    size_t read_count = fread(buffer, 1, sizeof buffer, input);
    if (read_count != 0) {
        if (fwrite(buffer, 1, read_count, output) != read_count) {
            /* error */
        }
        bytes += read_count;
    }
    if (read_count < sizeof buffer) {
        if (ferror(input)) { /* error */ }
        break;
    }
}
```

Файл открывается в режимах `"rb"` и `"wb"`, поэтому копируются именно
байты, а не текст: переводы строк, UTF-8 и нулевые байты не меняются.
Фактический вывод:

```text
$ ./filetool/filetool --input-file tests/sample_input.txt \
    --output-file artifacts/copy.txt --show-stats
Copied: 131 bytes
cmp: output is byte-identical to input
```

`--show-stats` печатает сообщение в `stdout`, а ошибки — в `stderr`.
Содержимое выходного файла — это данные, а `Copied: ...` — диагностическое
сообщение программы. Их можно разделить перенаправлением.

---

## 5. Часть III. `stdin`, `stdout`, `stderr` и перенаправление

| Поток | Назначение | В проекте |
|---|---|---|
| `stdin` | стандартный ввод | `--query -` читает запрос из stdin |
| `stdout` | обычный результат | таблицы `SELECT`, статистика, `Copied:` |
| `stderr` | диагностика и ошибки | сообщения об ошибках, `Usage` |

Скрипт `filetool/streams_demo.sh` демонстрирует все формы:

```bash
./filetool/filetool --input-file in.txt --output-file copy.txt \
    --show-stats > stdout.txt 2> stderr.txt

./filetool/filetool --input-file in.txt --output-file copy2.txt \
    --show-stats >> stdout.txt 2>> stderr.txt

./filetool/filetool --vot-takogo-vida \
    > unknown_stdout.txt 2> errors.txt

./filetool/filetool --input-file missing.txt \
    --output-file x.txt > all.txt 2>&1

./filetool/filetool --input-file in.txt --output-file copy3.txt \
    --show-stats | tr '[:lower:]' '[:upper:]'
```

Фактический результат:

```text
=== > and >> : stdout.txt ===
Copied: 17 bytes
Copied: 17 bytes
=== stderr.txt (expected empty) ===
=== 2> : unknown_stdout.txt (expected empty) ===
=== 2> : errors.txt ===
Error: unknown option: --vot-takogo-vida
Usage: filetool --input-file <file> --output-file <file> [--show-stats]
...
unknown option exit code: 64
=== 2>&1 : all.txt ===
Error: cannot open input file artifacts/streams/missing.txt: No such file or directory
missing input exit code: 66
=== | : pipe.txt ===
COPIED: 17 BYTES
```

Что здесь важно:

- `>` перезаписывает файл, `>>` добавляет в конец;
- `2>` перенаправляет только `stderr`, поэтому `unknown_stdout.txt`
  остаётся пустым;
- `2>&1` объединяет оба потока в один файл `all.txt`;
- в конвейере `filetool` не знает, что его `stdout` читает `tr`;
  он просто пишет в стандартный вывод.

Конвейер с базой данных:

```bash
printf 'SELECT students WHERE id = 304\n' | \
    ./database/vpddb --database artifacts/students.dat --query -
```

Вывод:

```text
id	name	group_id
304	Ivan Smirnov	12
```

---

## 6. Часть IV. От структуры к базе данных

### 6.1. Почему одна огромная структура — плохая идея

```c
typedef struct {
    uint32_t student_id;
    char student_name[64];
    char group_name[32];
    char faculty_name[64];
    char dean_name[64];
} Student;
```

Если десятки студентов учатся в одной группе, название группы и
факультета дублируется в каждой записи. Возникают аномалии:

- **аномалия вставки** — нельзя добавить факультет, пока нет студента;
- **аномалия обновления** — переименование факультета требует обновить
  все строки студентов;
- **аномалия удаления** — удаление последнего студента группы уничтожает
  сведения о группе и факультете.

Поэтому в проекте три связанные сущности.

### 6.2. Схема

```text
Faculty
-------
id (PK)
name

Group
-----
id (PK)
name
faculty_id (FK → Faculty.id)

Student
-------
id (PK)
name
group_id (FK → Group.id)
```

Структуры C:

```c
typedef struct {
    uint32_t id;
    char name[VPD_FACULTY_NAME_CAP];   /* 64 */
} Faculty;

typedef struct {
    uint32_t id;
    char name[VPD_GROUP_NAME_CAP];     /* 32 */
    uint32_t faculty_id;
} Group;

typedef struct {
    uint32_t id;
    char name[VPD_STUDENT_NAME_CAP];   /* 64 */
    uint32_t group_id;
} Student;
```

### 6.3. 1НФ, 2НФ, 3НФ

**1НФ.** В каждом поле одно скалярное значение. Нет `phone1`, `phone2`,
`phone3` и нет списка курсов в одной строке. Имя — одна строка, `id` —
одно число, внешний ключ — одно число.

**2НФ.** У всех трёх таблиц простой первичный ключ `id`, а не составной
`(student_id, course_id)`. Поэтому неключевые поля (`name`, `group_id`,
`faculty_id`) зависят от всего ключа, и частичных зависимостей нет.

**3НФ.** В `Student` нет названия группы или факультета — только
`group_id`. В `Group` нет названия факультета — только `faculty_id`.
Названия хранятся там, где они описывают сущность: название факультета —
в `Faculty`, название группы — в `Group`. Транзитивной зависимости
`student_id → group_id → faculty_id → faculty_name` в одной таблице нет:
цепочка проходит через разные таблицы и внешние ключи.

Именно поэтому переименование факультета — это одно обновление строки
`Faculty`, а не обновление всех студентов.

### 6.4. Первичный и внешний ключ

`Student.id` — первичный ключ: он однозначно идентифицирует запись.
`Student.group_id` — внешний ключ, ссылающийся на `Group.id`.

Кто гарантирует, что студент не получит `group_id = 999`, если группы с
таким `id` нет? В учебной системе это проверяет сам код:

```c
if (db_group_find(db, group_id) == NULL) {
    db_error(err, errlen,
             "foreign key violation: group with id %u does not exist",
             group_id);
    return -1;
}
```

Такие же проверки есть для `Group.faculty_id`, для обновления ссылок и
для удаления:

- нельзя удалить факультет, пока на него ссылаются группы;
- нельзя удалить группу, пока на неё ссылаются студенты;
- нельзя обновить `group_id` студента на несуществующую группу;
- нельзя добавить студента в несуществующую группу.

Это и есть реализация ссылочной целостности на уровне собственной
программы, без внешней СУБД.

---

## 7. Часть V. CRUD

### 7.1. Функции

```c
int db_faculty_insert(...);
int db_group_insert(...);
int db_student_insert(...);

const Faculty *db_faculty_find(const Database *db, uint32_t id);
const Group   *db_group_find(const Database *db, uint32_t id);
const Student *db_student_find(const Database *db, uint32_t id);

int db_faculty_update(...);
int db_group_update(...);
int db_student_update(...);

int db_faculty_delete(...);
int db_group_delete(...);
int db_student_delete(...);
```

Каждая изменяющая операция проверяет:

1. уникальность первичного ключа;
2. существование внешнего ключа;
3. длину и допустимость имени;
4. ссылочную целостность при удалении.

`INSERT ... 0 "Name" ...` означает «выдать следующий свободный id».
Фактический идентификатор возвращается в `assigned_id`, и исполнитель
печатает его:

```text
$ ./database/vpddb --database artifacts/students.dat \
    --query 'INSERT student 0 "Auto Id" 11'
inserted student id=401
```

### 7.2. Сохранение после изменений

После каждого `INSERT`, `UPDATE` или `DELETE` программа сохраняет базу
через `storage_save`. Поэтому состояние переживает завершение процесса:

```text
$ ./database/vpddb --database students.dat \
    --query 'INSERT student 400 "Test Student" 11'
inserted student id=400

$ ./database/vpddb --database students.dat \
    --query 'SELECT students WHERE id = 400'
id	name	group_id
400	Test Student	11
```

Это и есть использование файлового слоя из ЛР4: структуры записываются
в двоичный файл и читаются обратно при следующем запуске.

### 7.3. Индекс `id → позиция` (повышенный уровень)

Помимо массивов записей, в `Database` есть три хеш-индекса:

```c
typedef struct {
    uint32_t id;
    size_t position;
    unsigned char state; /* empty / used / tombstone */
} IdIndexEntry;

typedef struct {
    IdIndexEntry *entries;
    size_t capacity;
    size_t size;
    size_t tombstones;
} IdIndex;
```

Используется открытая адресация с linear probing, надгробиями и
перехешированием при заполнении около 70 %. Поиск `db_student_find`
не просматривает весь массив, а идёт по хеш-цепочке.

При удалении последняя запись массива переносится на место удалённой,
а её позиция обновляется в индексе:

```c
if (position != last) {
    db->students[position] = db->students[last];
    update_position_after_swap(&db->student_index,
                               db->students[position].id, position, ...);
}
--db->student_count;
```

`db_validate` проверяет, что каждая запись действительно находится по
той позиции, на которую указывает индекс, и что размер индекса совпадает
с числом записей.

---

## 8. Часть VI. Двоичное хранилище

### 8.1. Формат файла

```text
StorageHeader (48 байт)
  magic[8] = "VPDDB05"
  version          = 1
  header_size      = 48
  payload_size
  payload_checksum (FNV-1a)
  faculty_count
  group_count
  student_count
  next_faculty_id
  next_group_id
  next_student_id
Payload
  FileFaculty  × faculty_count   (68 байт каждая)
  FileGroup    × group_count     (40 байт каждая)
  FileStudent  × student_count   (72 байт каждая)
```

Размеры зафиксированы `_Static_assert`:

```c
_Static_assert(sizeof(StorageHeader) == 48, "unexpected StorageHeader size");
_Static_assert(sizeof(FileFaculty) == 68, "unexpected FileFaculty size");
_Static_assert(sizeof(FileGroup) == 40, "unexpected FileGroup size");
_Static_assert(sizeof(FileStudent) == 72, "unexpected FileStudent size");
```

Формат версионирован: поле `version` и `header_size` позволяют отличить
старую версию от новой и не читать файл «наугад». Двоичный формат
рассчитан на ту же платформу/ABI (нативный порядок байтов, фиксированные
размеры полей); для переноса между разными системами используется JSON.

### 8.2. Проверки при загрузке

`storage_load` проверяет:

1. файл не короче заголовка;
2. magic равен `VPDDB05`;
3. версия поддерживается;
4. `header_size` совпадает с ожидаемым;
5. `payload_size` совпадает с реальным размером файла;
6. количество записей соответствует размеру payload;
7. контрольная сумма FNV-1a совпадает;
8. имена NUL-terminated, непустые и без управляющих символов;
9. первичные ключи уникальны;
10. внешние ключи существуют;
11. индексы согласованы с массивами.

Фактические сообщения:

```text
corrupted database ...: bad magic
corrupted database ...: checksum mismatch (stored 0x67a1c934, actual 0x2d7b8ba9)
corrupted database ...: file is shorter than header
```

Коды завершения — `75`.

### 8.3. Атомарное сохранение

Запись идёт не в основной файл, а во временный:

```text
<path>.tmp  →  fflush  →  fsync  →  fclose  →  rename(<path>.tmp, <path>)
```

Если процесс упадёт посреди записи, старый `<path>` останется целым.
`rename` в POSIX в пределах одной файловой системы атомарен. Это простая
и понятная стратегия сохранения состояния без полноценного WAL.

### 8.4. Журнал изменений

Перед каждым мутирующим запросом `INSERT`/`UPDATE`/`DELETE` программа
дописывает строку в `<path>.journal`:

```text
# VPD Lab 05 journal
[2026-11-17T20:31:12] INSERT: INSERT student 400 "Test Student" 11
[2026-11-17T20:31:13] UPDATE: UPDATE students SET group_id = 12 WHERE id = 400
[2026-11-17T20:31:14] DELETE: DELETE students WHERE id = 400
```

Это audit log, а не полноценный write-ahead log с автоматическим replay:
он позволяет увидеть историю изменений и порядок операций. Полноценный
WAL с восстановлением — задача промышленной СУБД, а не учебной работы.

---

## 9. Часть VII. JSON

### 9.1. `statham.json`

В файле шесть цитат, приписываемых Джейсону Стейтему. Все они помечены
`"verified": false`, потому что надёжного первоисточника у мемных цитат
нет. В файле встречаются все основные типы JSON:

- строка — `"author": "Jason Statham"`;
- число — `"quote_count": 6`, `"times_quoted": 1200`;
- логическое значение — `"memes_are_verified": false`;
- массив — `"tags": ["action", "memes", "life"]`, `"quotes": [...]`;
- объект — корневой объект и объекты цитат;
- `null` — `"primary_source": null`.

Проверка парсером:

```text
$ ./json/json_demo json/statham.json
file: json/statham.json
root: object
types: object=7 array=2 string=17 number=7 boolean=7 null=1
author: Jason Statham
subject: internet memes
quote_count field: 6
memes_are_verified: false
tags (3): action memes life
primary_source type: null (value: null)
quotes: 6
quote[0]: verified=false category=life times_quoted=1200 text="I don't need a gym; I just carry my own ego."
...
```

`json.c` — строгий рекурсивный парсер. Он поддерживает объекты, массивы,
строки с `\"`, `\\`, `\n`, `\t`, `\uXXXX` и суррогатными парами, числа,
`true`, `false`, `null`. Он отвергает:

- trailing comma;
- ведущий ноль в числе (`01`);
- нечисловые `NaN`/`Infinity`;
- незакрытые строки и объекты;
- неизвестные escape-последовательности;
- дублирующиеся ключи объекта;
- лишние данные после корневого значения;
- вложенность глубже 64 уровней.

### 9.2. Экспорт базы

```bash
./database/vpddb --database students.dat --export json/export.json
```

Фрагмент `json/export.json`:

```json
{
  "format": "vpd-lab05",
  "version": 1,
  "faculties": [
    {
      "id": 1,
      "name": "Institute of Information Technology"
    }
  ],
  "groups": [
    {
      "id": 11,
      "name": "M3O-119BV-26",
      "faculty_id": 1
    }
  ],
  "students": [
    {
      "id": 301,
      "name": "Ivan Ivanov",
      "group_id": 11
    }
  ]
}
```

JSON — формат обмена и экспорта, а не внутренний формат хранения.
Внутри база хранится в двоичном файле с заголовком, проверками и
атомарной записью.

### 9.3. Импорт

```bash
./database/vpddb --database new.dat --init --import json/export.json
```

Импорт строится во временной `Database`:

```c
Database temporary;
db_init(&temporary);

if (import_faculties(&temporary, faculties, ...) != 0 ||
    import_groups(&temporary, groups, ...) != 0 ||
    import_students(&temporary, students, ...) != 0 ||
    db_validate(&temporary, ...) != 0) {
    db_free(&temporary);   /* original database is untouched */
    return JSON_DB_ERROR_DATA;
}

db_free(db);
*db = temporary;           /* replace only after full success */
```

Поэтому повреждённый JSON или битый внешний ключ не портит текущую базу.
Ошибки различаются:

- синтаксис JSON — код `65`;
- нарушение ссылочной целостности — код `74`;
- отсутствующий файл — код `66`.

### 9.4. Сериализация и десериализация

**Сериализация** — преобразование внутреннего представления (массивов
структур `Faculty`, `Group`, `Student`) в формат обмена (JSON-текст,
двоичный файл). **Десериализация** — обратное восстановление структур из
формата обмена с проверкой типов и инвариантов.

В проекте есть два разных формата:

| Формат | Роль | Особенности |
|---|---|---|
| двоичный `*.dat` | внутреннее хранение | компактность, checksum, версия, атомарная запись |
| JSON `*.json` | обмен/экспорт | читаемость, переносимость, возможность проверить глазами |

Это разные задачи, поэтому и форматы разные.

### 9.5. Почему PostgreSQL не хранит всю базу в одном `database.json`

Если бы PostgreSQL читал и перезаписывал один JSON-файл целиком, он бы
потерял почти всё, за что отвечает СУБД:

- **Поиск.** Пришлось бы разбирать весь файл и просматривать все записи;
  нельзя использовать B-дерево, хеш-индекс или планировщик запросов.
- **Изменение отдельных записей.** Любой `UPDATE` требовал бы
  перезаписи всего файла, а не страницы/строки.
- **Индексы.** JSON-файл не даёт страничной организации, индексов и
  статистики; индексы пришлось бы каждый раз строить заново.
- **Параллельная работа.** Нельзя аккуратно дать нескольким транзакциям
  читать и писать одновременно: файл пришлось бы блокировать целиком.
- **Транзакции.** Нет атомарности нескольких изменений, изоляции,
  отката, MVCC и блокировок на уровне строк.
- **Восстановление после сбоя.** Нет WAL, контрольных точек, checksum
  страниц, резервного копирования и replay. Один сбой посреди перезаписи
  может уничтожить весь файл.
- **Контроль целостности.** Ограничения, внешние ключи, триггеры и типы
  должны проверяться ядром СУБД, а не приложением.
- **Масштаб.** База на терабайты не помещается в память и не должна
  переписываться целиком из-за одной строки.
- **Конкурентность и репликация.** Нужны журналы, блокировки, снимки,
  репликация и восстановление на момент времени.

JSON удобен как формат обмена, конфигурации и экспорта. PostgreSQL же
использует страничное хранение, WAL, буферный кэш, индексы и
транзакционный движок. Поэтому «вся база в одном `database.json`» — это
не СУБД, а простой файл с данными.

---

## 10. Часть VIII. YAML

`config.yaml` содержит тот же простой набор параметров, что и JSON:

```yaml
# YAML
database: students.dat
export_file: json/export.json
show_stats: false
```

```json
{
  "database": "students.dat",
  "export_file": "json/export.json",
  "show_stats": false
}
```

Отличия:

- в YAML нет фигурных скобок и кавычек для простых скаляров;
- структура задаётся отступами (в учебном файле — пробелами, не
  табуляцией);
- комментарии начинаются с `#`;
- JSON строже к кавычкам, запятым и типам.

`database/config.c` — минимальный разбор плоского YAML:

```text
ключ: значение
```

Поддерживаются `database`, `export_file`, `show_stats` (`true`/`false`),
комментарии и необязательные кавычки. Вложенность и табуляция не
поддерживаются: в лабораторной YAML нужен только для сравнения с JSON,
а не как полноценная замена.

Запуск:

```bash
./database/vpddb --config config.yaml --export
./database/vpddb --config config.yaml --database students.dat \
    --query 'SELECT faculties'
```

Явный `--database` перекрывает значение из конфигурации. Если `--export`
указан без имени файла, используется `export_file` из YAML.

---

## 11. Часть IX. Собственный язык запросов

### 11.1. Грамматика

```text
SELECT   <table> [WHERE <field> <op> <value> [AND ...]]
                 [ORDER BY <field> [ASC|DESC]]
INSERT   <table> <id> "<name>" [<foreign_key>]
UPDATE   <table> SET <field> = <value> [, ...]
                 WHERE <field> <op> <value> [AND ...]
DELETE   <table> WHERE <field> <op> <value> [AND ...]
```

Таблицы: `faculty`/`faculties`, `group`/`groups`, `student`/`students`.
Поля: `id`, `name`, `group_id`, `faculty_id` — но только те, которые
принадлежат выбранной таблице. Операторы: `=`, `!=`, `<`, `<=`, `>`, `>=`.
Ключевые слова регистронезависимы.

Примеры:

```text
INSERT student 304 "Ivan Ivanov" 12
SELECT students
SELECT students WHERE id = 304
SELECT students WHERE group_id = 12
SELECT students WHERE group_id = 12 AND name = "Ivan Ivanov"
SELECT students WHERE group_id = 12 ORDER BY name DESC
UPDATE students SET group_id = 13 WHERE id = 304
DELETE students WHERE id = 304
```

Ограничения безопасности:

- `UPDATE` и `DELETE` обязаны содержать `WHERE`;
- `SET id = ...` запрещён — первичный ключ не меняется;
- имя можно сравнивать только через `=` и `!=`;
- максимум 4 условия `AND` и 4 присваивания `SET`;
- `INSERT` принимает ровно столько значений, сколько колонок в таблице.

Главная идея DSL: пользователь описывает **что** хочет получить или
изменить, а не **как** перебирать массив. Декларативная команда
`SELECT students WHERE group_id = 12` внутри реализуется императивным
циклом C.

### 11.2. Лексический разбор

Для запроса

```text
SELECT students WHERE group_id = 12
```

лексер выдаёт токены:

```text
SELECT | students | WHERE | group_id | = | 12
```

В коде токен — это структура:

```c
typedef struct {
    TokenType type;
    char text[VPD_QUERY_TEXT_CAP];
    uint32_t number;
    size_t position;
} Token;
```

`lexer.c` распознаёт:

- слова (`WORD`) — идентификаторы и ключевые слова;
- целые числа (`NUMBER`) с проверкой диапазона `uint32_t`;
- строки (`STRING`) с escape-последовательностями;
- `=`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `,`, `*`, `;`;
- позицию каждого токена для сообщений об ошибках.

Лексер не решает, «правильный ли это запрос». Он только отвечает на
вопрос «какие элементы находятся в строке». Например, `SELECT` и
`students` — оба слова; смысл появляется на этапе парсера.

Ошибки лексера:

```text
lexer error at position 5: unexpected character '#'
lexer error at position 3: unterminated string literal
lexer error at position 8: number does not fit into uint32_t
```

### 11.3. Синтаксический разбор

Парсер проверяет порядок токенов и строит внутреннее представление:

```c
typedef struct {
    QueryCommand command;     /* SELECT / INSERT / UPDATE / DELETE */
    QueryTable table;         /* FACULTY / GROUP / STUDENT */
    QueryValue insert_values[3];
    size_t insert_value_count;
    QueryAssignment assignments[4];
    size_t assignment_count;
    QueryCondition conditions[4];
    size_t condition_count;
    int has_order;
    QueryField order_field;
    int order_desc;
} Query;
```

Для запроса

```text
SELECT students WHERE group_id = 12
```

получается:

```text
Query {
    command    = SELECT,
    table      = STUDENT,
    conditions = [
        { field = GROUP_ID, compare = EQ, value = 12 }
    ],
    has_order  = 0
}
```

Парсер — рекурсивный спуск. Он проверяет:

- команда — известное слово;
- таблица — `faculty`, `group` или `student` (в единственном или
  множественном числе);
- поле принадлежит таблице;
- тип значения соответствует полю (`name` — строка, id и FK — число);
- `ORDER BY` идёт после `WHERE` и содержит допустимое поле;
- после запроса нет лишних токенов (кроме необязательной `;`).

Ошибки парсера:

```text
parser error at position 7: unknown table 'teachers'
parser error at position 22: field 'faculty_id' does not belong to table 'student'
parser error at position 23: primary key id cannot be updated
parser error at position 15: DELETE requires a WHERE clause
```

### 11.4. Выполнение

Исполнитель `query_execute` преобразует AST в операции над структурами.
Для `SELECT`:

```c
for (size_t i = 0; i < db->student_count; ++i) {
    if (match_student(&db->students[i], query)) {
        rows[count++] = &db->students[i];
    }
}
```

Затем при наличии `ORDER BY` массив указателей сортируется вставками
(для учебных объёмов это проще и предсказуемее, чем `qsort` с
глобальным контекстом), а результат печатается в TSV:

```text
id	name	group_id
301	Ivan Ivanov	11
302	Petr Petrov	11
```

Для `UPDATE` и `DELETE` сначала собираются идентификаторы всех
подходящих записей, и только потом выполняются изменения:

```c
collect_student_ids(db, query, &ids, &count, ...);
for (size_t i = 0; i < count; ++i) {
    db_student_update(db, ids[i], ...);
}
```

Это важно, потому что изменение `group_id` может повлиять на условие
`WHERE`, а удаление сдвигает элементы массива. Сбор id заранее делает
поведение предсказуемым.

Полная цепочка:

```text
декларативный запрос
        ↓
лексический разбор (lexer.c)
        ↓
синтаксический разбор (parser.c)
        ↓
Query (внутреннее представление)
        ↓
императивный C-код (executor.c)
        ↓
результат в stdout
```

---

## 12. Часть X. Обработка ошибок и целостность

Минимально обработанные ситуации:

| Ситуация | Сообщение / код |
|---|---|
| неизвестный параметр CLI | `unknown option`, `64` |
| нет значения после параметра | `requires ...`, `64` |
| файл базы не открывается | `cannot read database`, `66` |
| дубликат первичного ключа | `already exists`, `74` |
| несуществующий внешний ключ | `foreign key violation`, `74` |
| неизвестная команда запроса | `unknown command`, `65` |
| неизвестная таблица | `unknown table`, `65` |
| неполный запрос | `parser error`, `65` |
| поле не из той таблицы | `does not belong to table`, `65` |
| удаление нарушает ссылку | `cannot delete ... references it`, `74` |
| повреждённый JSON | `JSON error at byte ...`, `65` |
| JSON с плохим FK | `foreign key violation`, `74` |
| повреждённый заголовок | `bad magic`, `75` |
| неверная контрольная сумма | `checksum mismatch`, `75` |
| обрезанный файл | `file is shorter than header`, `75` |

Все ошибки печатаются в `stderr`; нормальные результаты — в `stdout`.
Это позволяет отделять данные от диагностики при перенаправлении и
использовать программу в скриптах.

Пример:

```text
$ ./database/vpddb --database students.dat \
    --query 'INSERT student 301 "Duplicate" 11'
Execution error: student with id 301 already exists
exit=74

$ ./database/vpddb --database students.dat \
    --query 'DELETE groups WHERE id = 11'
Execution error: cannot delete group 11: student 301 references it
exit=74
```

---

## 13. Тестирование

Автоматические тесты:

```bash
make test
```

Результат:

```text
Total: 100 passed, 0 failed
```

Проверяются:

- обе формы `main`, `argc`, `argv`, аргумент с пробелом;
- копирование файла и побайтовое сравнение;
- коды `64`, `66`, `73`;
- `>`, `>>`, `2>`, `2>&1`, конвейер;
- инициализация, seed, статистика, проверка целостности;
- `SELECT`, `WHERE`, `AND`, `ORDER BY`, `INSERT`, `UPDATE`, `DELETE`;
- автоматический id;
- дубликат PK, плохой FK, запрет удаления связанной группы;
- журнал изменений;
- экспорт и импорт JSON;
- повреждённый JSON, плохой FK в JSON;
- повреждённые magic, checksum, размер файла;
- ошибки лексера и парсера;
- stdin-запрос, query-файл, конфигурация YAML, persistence.

ASan/UBSan:

```bash
make sanitize
```

Собираются `artifacts/vpddb_asan`, `artifacts/json_demo_asan`,
`artifacts/filetool_asan`; выполняются seed, SELECT, INSERT, export,
разбор `statham.json` и копирование файла. Ошибок ASan/UBSan нет.

Артефакты:

| Файл | Содержимое |
|---|---|
| `artifacts/01_argv.txt` | `argc`, `argv`, `main(void)` |
| `artifacts/02_filetool.txt` | копирование, 131 байт, `cmp` |
| `artifacts/03_streams.txt` | `>`, `>>`, `2>`, `2>&1`, pipe |
| `artifacts/04_init_seed.txt` | 2 факультета, 3 группы, 5 студентов |
| `artifacts/05_session.txt` | INSERT/UPDATE/DELETE/SELECT |
| `artifacts/06_select.txt` | SELECT + ORDER BY |
| `artifacts/07_stats.txt` | счётчики и 664 байта |
| `artifacts/08_check.txt` | `integrity: ok` |
| `artifacts/09_export.txt` | экспорт JSON |
| `artifacts/10_import.txt` | импорт JSON |
| `artifacts/11_json_statham.txt` | все типы JSON, 6 цитат |
| `artifacts/12_json_export.txt` | сводка по `export.json` |
| `artifacts/13_errors.txt` | ошибки и exit-коды |
| `artifacts/14_corruption.txt` | bad magic, checksum, truncation |
| `artifacts/15_config.txt` | запуск с YAML |
| `artifacts/16_pipeline.txt` | запрос через stdin |
| `artifacts/asan_*.txt` | прогоны ASan/UBSan |

---

## 14. Повышенный уровень

Реализованы следующие расширения:

1. **Индекс `id → позиция`** для всех трёх сущностей на основе открытой
   адресации с linear probing, надгробиями и перехешированием.
2. **Поиск по идентификатору без полного просмотра** — `db_*_find`
   обращается к хеш-индексу, а не к циклу по массиву.
3. **Проверка ссылочной целостности** между тремя сущностями при
   вставке, обновлении и удалении.
4. **Версионирование двоичного формата** — `version` и `header_size` в
   заголовке.
5. **Обнаружение повреждений** — bad magic, неверный размер, checksum
   FNV-1a, проверка количества записей.
6. **Журнал изменений** перед изменением основного файла —
   `<path>.journal`.
7. **Несколько условий `AND`** в `WHERE`.
8. **`ORDER BY`** с `ASC`/`DESC` для всех допустимых полей.
9. **Атомарное сохранение** через `<path>.tmp` + `fsync` + `rename`.
10. **Автоматические идентификаторы** при `INSERT ... 0 ...`.

Не реализовывались B-дерево, полноценный SQL, ACID/MVCC, сетевой сервер
и промышленная СУБД — по условию работы это не требуется.

---

## 15. Выводы

В работе получилась небольшая, но полная цепочка:

```text
CLI → потоки → файловая база → CRUD → JSON → YAML → DSL → исполнение
```

- `argc`/`argv` дают программе интерфейс командной строки, а коды
  завершения — способ сообщить о результате.
- `stdin`/`stdout`/`stderr` и перенаправления позволяют отделить данные
  от диагностики и соединять программы конвейером.
- Три связанные сущности с первичными и внешними ключами устраняют
  дублирование и аномалии обновления/удаления; схема соответствует
  смыслу 1НФ–3НФ.
- Двоичный файл с заголовком, версией, checksum и атомарной записью
  даёт постоянное хранение, а JSON — удобный формат обмена.
- Собственный DSL с лексером, парсером и исполнителем показывает, как
  декларативная команда превращается в императивные операции над
  структурами C.
- Обработка ошибок и проверки целостности не дают «тихо» испортить
  данные: каждая ошибка имеет свой код и сообщение в `stderr`.

Все обязательные пункты задания выполнены, дополнительные расширения
(индекс, несколько условий, `ORDER BY`, версия формата, checksum,
журнал, атомарная запись) реализованы и проверены автоматическими
тестами и ASan/UBSan.
