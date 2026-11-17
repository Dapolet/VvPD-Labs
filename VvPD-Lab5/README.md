# Лабораторная работа № 5

**Командная строка, файловая база данных и собственный язык запросов**

Проект показывает полную цепочку:

```text
main() → argc/argv → CLI → stdin/stdout/stderr → CRUD →
двоичный файл → JSON → YAML → лексер → парсер → исполнитель DSL
```

Все эксперименты выполнены на macOS 15.8.1 (arm64, Apple M1 Pro).
Основной компилятор — Homebrew GCC 16.2.0 (`gcc-16`); для ASan/UBSan
используется Apple Clang 16.0.0.

## Окружение

| Инструмент | Версия |
|---|---|
| ОС | macOS 15.8.1 (24H32), arm64 |
| Процессор | Apple M1 Pro |
| GCC | Homebrew GCC 16.2.0 (`gcc-16`) |
| Apple Clang | 16.0.0 (clang-1600.0.26.6) |
| GNU Make | 3.81 |
| Стандарт | C17 (`-std=c17`) |
| Предупреждения | `-Wall -Wextra -Wpedantic` |
| Санитайзеры | ASan + UBSan, Apple Clang 16 |

На macOS команда `gcc` обычно является Apple Clang, поэтому Makefile
автоматически выбирает `gcc-16`, если он есть в `PATH`.

## Структура

```text
VvPD-Lab5/
├── filetool/
│   ├── filetool.c          # CLI, копирование, --show-stats, --show-args
│   ├── argv_demo.c         # int main(void) и int main(int argc, char **argv)
│   └── streams_demo.sh     # >, >>, 2>, 2>&1, |
├── database/
│   ├── model.h/model.c     # Faculty, Group, Student, CRUD, индексы, проверки
│   ├── storage.h/storage.c # двоичный формат, checksum, atomic save, journal
│   ├── database.h/database.c # seed и прикладные обёртки
│   ├── config.h/config.c   # минимальный разбор YAML-подобного config
│   └── main.c              # CLI мини-СУБД
├── json/
│   ├── statham.json        # 6 цитат, все типы JSON
│   ├── export.json         # экспорт учебной базы
│   ├── json.h/json.c       # строгий JSON-парсер
│   ├── database_json.h/.c  # export/import базы
│   └── json_demo.c         # разбор statham.json и подсчёт типов
├── query/
│   ├── query.h             # токены, AST, API
│   ├── lexer.c             # лексический анализатор
│   ├── parser.c            # синтаксический анализатор
│   └── executor.c          # выполнение запроса над Database
├── tests/
│   ├── run_tests.sh        # 100 автоматических проверок
│   ├── make_artifacts.sh   # генерация artifacts/
│   ├── demo_session.txt    # демонстрационная сессия запросов
│   ├── test_cases.txt      # описание тестовых случаев
│   └── sample_input.txt    # вход filetool
├── artifacts/              # сохранённые выводы и результаты ASan
├── config.yaml             # YAML-конфигурация
├── Makefile
├── README.md
└── REPORT.md
```

## Быстрый старт

```bash
make            # собрать все программы
make test       # 100 регрессионных проверок
make artifacts  # сохранить выводы в artifacts/
make sanitize   # ASan/UBSan для vpddb, json_demo и filetool
make clean      # удалить исполняемые файлы и базы
```

## filetool

```bash
./filetool/filetool --input-file tests/sample_input.txt \
    --output-file /tmp/copy.txt --show-stats
```

Вывод:

```text
Copied: 131 bytes
```

Параметры:

| Параметр | Назначение |
|---|---|
| `--input-file <file>` | входной файл |
| `--output-file <file>` | выходной файл |
| `--show-stats` | напечатать количество скопированных байт |
| `--show-args` | напечатать `argc` и все `argv[i]` |
| `--help` | справка |

Коды завершения:

| Код | Класс ошибки |
|---|---|
| `0` | успех |
| `64` | ошибка командной строки |
| `66` | входной файл не открывается |
| `73` | выходной файл не открывается |
| `74` | ошибка чтения/записи |

Формы `main`:

```bash
./filetool/argv_demo alpha "beta gamma"   # int main(int argc, char *argv[])
./filetool/argv_void_demo                 # int main(void)
```

Потоки и перенаправления:

```bash
bash filetool/streams_demo.sh artifacts/streams
```

Демонстрация показывает `>`, `>>`, `2>`, `2>&1` и конвейер
`filetool ... --show-stats | tr a-z A-Z`.

## Мини-СУБД

Инициализация и демонстрационные данные:

```bash
./database/vpddb --database students.dat --init --seed --show-stats
```

Вывод:

```text
initialized database: students.dat
database: students.dat
faculties: 2
groups: 3
students: 5
index: open addressing, id -> position
file_bytes: 664
```

Запросы:

```bash
./database/vpddb --database students.dat --query 'SELECT students'
./database/vpddb --database students.dat \
    --query 'SELECT students WHERE group_id = 11 ORDER BY name'
./database/vpddb --database students.dat \
    --query 'INSERT student 306 "Nikolai Orlov" 11'
./database/vpddb --database students.dat \
    --query 'UPDATE students SET group_id = 12 WHERE id = 306'
./database/vpddb --database students.dat \
    --query 'DELETE students WHERE id = 306'
```

Чтение запроса из stdin:

```bash
echo 'SELECT students WHERE id = 304' | \
    ./database/vpddb --database students.dat --query -
```

Пакетный режим:

```bash
./database/vpddb --database students.dat --file tests/demo_session.txt
```

JSON и YAML:

```bash
./database/vpddb --database students.dat --export json/export.json
./database/vpddb --database new.dat --init --import json/export.json
./json/json_demo json/statham.json
./database/vpddb --config config.yaml --export   # путь export_file из YAML
./database/vpddb --config config.yaml --database students.dat \
    --query 'SELECT faculties'
```

## Схема данных

```text
Faculty(id, name)
Group(id, name, faculty_id → Faculty.id)
Student(id, name, group_id → Group.id)
```

- `id` — первичный ключ, проверяется на уникальность;
- `faculty_id` и `group_id` — внешние ключи;
- нельзя добавить группу с несуществующим факультетом;
- нельзя добавить студента в несуществующую группу;
- нельзя удалить факультет, на который ссылаются группы;
- нельзя удалить группу, на которую ссылаются студенты;
- `UPDATE ... SET id = ...` запрещён: первичный ключ не меняется.

Схема соответствует 1НФ (скалярные поля), 2НФ (простые первичные ключи,
нет частичных зависимостей) и 3НФ (названия факультетов и групп хранятся
только в своих таблицах, транзитивных зависимостей нет).

## Язык запросов

```text
SELECT   <table> [WHERE <field> <op> <value> [AND ...]] [ORDER BY <field> [ASC|DESC]]
INSERT   <table> <id> "<name>" [<foreign_key>]
UPDATE   <table> SET <field> = <value> [, ...] WHERE <field> <op> <value> [AND ...]
DELETE   <table> WHERE <field> <op> <value> [AND ...]
```

Таблицы: `faculty`/`faculties`, `group`/`groups`, `student`/`students`.
Поля: `id`, `name`, `group_id`, `faculty_id` (только в своей таблице).
Операторы: `=`, `!=`, `<`, `<=`, `>`, `>=`.
Строки записываются в двойных кавычках; поддерживаются escape-последовательности
`\"`, `\\`, `\n`, `\t`, `\uXXXX`.

`INSERT ... 0 "Name" ...` выдаёт следующий свободный идентификатор.

## Двоичный формат

```text
StorageHeader (48 байт)
  magic "VPDDB05", version, header_size, payload_size,
  payload_checksum, counts, next_*_id
Payload
  FacultyRecord × N   (68 байт)
  GroupRecord   × N   (40 байт)
  StudentRecord × N   (72 байт)
```

Проверяются magic, версия, размер заголовка, размер payload, количества
записей, контрольная сумма FNV-1a, имена, первичные и внешние ключи,
а также согласованность индексов. Запись идёт в `<file>.tmp` с `fsync`
и атомарным `rename`, поэтому старый файл не теряется при сбое.
Перед каждым `INSERT`/`UPDATE`/`DELETE` в `<file>.journal` пишется
audit-строка с временем и текстом запроса.

## Индексы и производительность

Для каждой сущности поддерживается хеш-индекс `id → позиция` на основе
открытой адресации с linear probing, надгробиями и перехешированием.
Поэтому `student_find_by_id` не просматривает таблицу целиком.
При удалении последняя запись переносится на место удалённой, а индекс
обновляется. `db_validate` проверяет, что каждая запись находится именно
по той позиции, на которую указывает индекс.

## Тесты и артефакты

```bash
make test
```

Результат: `100 passed, 0 failed` (файл `artifacts/tests.txt`).

| Артефакт | Содержимое |
|---|---|
| `artifacts/01_argv.txt` | `argc`, `argv`, обе формы `main` |
| `artifacts/02_filetool.txt` | копирование и байты |
| `artifacts/03_streams.txt` | `>`, `>>`, `2>`, `2>&1`, pipe |
| `artifacts/04_init_seed.txt` | инициализация и seed |
| `artifacts/05_session.txt` | INSERT/UPDATE/DELETE/SELECT |
| `artifacts/06_select.txt` | SELECT + ORDER BY |
| `artifacts/07_stats.txt` | счётчики и размер файла |
| `artifacts/08_check.txt` | проверка целостности |
| `artifacts/09_export.txt` | экспорт JSON |
| `artifacts/10_import.txt` | импорт JSON |
| `artifacts/11_json_statham.txt` | разбор `statham.json` |
| `artifacts/12_json_export.txt` | разбор `export.json` |
| `artifacts/13_errors.txt` | ошибки CLI, CRUD, JSON и DSL |
| `artifacts/14_corruption.txt` | bad magic, checksum, truncation |
| `artifacts/15_config.txt` | запуск с `config.yaml` |
| `artifacts/16_pipeline.txt` | запрос через stdin/pipe |
| `artifacts/asan_*.txt` | прогоны под ASan/UBSan |

Подробное объяснение всех частей, включая 1НФ–3НФ, JSON/YAML,
PostgreSQL и внутреннее устройство DSL, находится в `REPORT.md`.
