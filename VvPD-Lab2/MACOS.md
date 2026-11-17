# Сборка и запуск на macOS

Документ описывает подготовку окружения и команды для лабораторной работы № 2
на macOS. Все команды выполняются из **корня репозитория** (каталог, где лежат
`Makefile` и этот файл), если не указано иное.

Проверено на macOS 15.8.1 (24H32, Apple Silicon, arm64) с Homebrew GCC 16.2.0,
Apple Clang 16.0.0, GNU Make 3.81, CMake 4.4.5 и Ninja 1.13.2. Версии могут
быть новее — команды не меняются.

## 1. Что понадобится

| Компонент | Зачем | Откуда |
| --- | --- | --- |
| Xcode Command Line Tools | `clang`, `cc`, `make`, линковщик и заголовки | Apple |
| Homebrew GCC 16 | C23-эксперименты; Makefile выбирает `gcc-16` автоматически | Homebrew |
| CMake | альтернативная сборка основного проекта | Homebrew (необязательно) |
| Ninja | генератор CMake | Homebrew (необязательно) |
| Инструменты Lab 1 | только для цели `make lab1` | см. `VvPD-Lab1/MAC_OS.md` |

Для основной части достаточно Command Line Tools и GCC. Apple Clang собирает
проект (`-std=c17`), но не подходит для строк матрицы C23 в `make standards`.

## 2. Установка инструментов

### 2.1. Xcode Command Line Tools

```bash
xcode-select --install
```

Если инструменты уже установлены, macOS покажет соответствующее сообщение.
Проверка:

```bash
xcode-select -p
clang --version
make --version
```

При необходимости принять лицензию:

```bash
sudo xcodebuild -license accept
```

### 2.2. Homebrew

Если Homebrew ещё нет:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

На Apple Silicon Homebrew находится в `/opt/homebrew`. Добавьте его в `PATH`
текущей сессии:

```bash
eval "$(/opt/homebrew/bin/brew shellenv)"
```

Чтобы настройка сохранилась в новых терминалах:

```bash
echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zshrc
```

На Intel Mac префикс Homebrew — `/usr/local`; используйте
`$(brew --prefix)` в командах, чтобы не зависеть от архитектуры.

### 2.3. GCC

```bash
brew install gcc
```

Homebrew устанавливает версионированный бинарник, например `gcc-16`. Именно
его ищет `Makefile`:

```bash
command -v gcc-16
gcc-16 --version
```

Если формула уже обновилась и в системе появился, например, `gcc-17`:

```bash
ls "$(brew --prefix gcc)/bin"/gcc-*
```

В этом случае передайте компилятор явно (см. раздел 8):

```bash
make CC="$(brew --prefix gcc)/bin/gcc-17"
```

`gcc` без суффикса на macOS — это Apple Clang, а не GNU GCC.

### 2.4. CMake и Ninja

Необязательно, только для альтернативной сборки:

```bash
brew install cmake ninja
```

### 2.5. Проверка окружения

```bash
clang --version
gcc-16 --version
make --version
cmake --version
ninja --version
uname -m          # arm64 — Apple Silicon, x86_64 — Intel
```

## 3. Быстрый старт

```bash
make            # собрать основной проект (-O0)
make run        # собрать и запустить
```

С настройками по умолчанию одна стадия обрабатывает `5 500 000` значений по
`64` раунда смешивания. Полный прогон на Apple M1 Pro занимает около 12
секунд.

Для быстрой проверки уменьшите нагрузку:

```bash
make -B ITEMS=1000000 ROUNDS=32 run
```

## 4. Основной проект

### 4.1. Сборка и запуск

```bash
make
./vpd_lab2
```

Или одной командой:

```bash
make run
```

`make` собирает `obj/main.o`, `obj/device.o`, `obj/timer.o` и линкует их в
`vpd_lab2`. По умолчанию используются `-std=c17 -Wall -Wextra -Wpedantic -O0 -g`.

Программа печатает стадии `for`, `while`, `do...while` и `bits`, прогресс-бар,
три измерения времени, `checksum`, значения `global_runs`, счётчики вызовов
`device_tick()` и маску `completed`.

### 4.2. Параметры сборки

Переопределить нагрузку:

```bash
make -B ITEMS=200000 ROUNDS=16 run
```

`make` не отслеживает изменения `ITEMS`, `ROUNDS` и `CFLAGS`: если бинарник уже
собран, он может остаться прежним. Флаг `-B` (или `make clean` перед сборкой)
гарантирует пересборку с новыми значениями.

Переопределить компилятор и флаги:

```bash
make -B CC=clang
make -B CFLAGS="-std=c17 -Wall -Wextra -Wpedantic -O2 -g"
```

Отладочная сборка без оптимизации уже используется по умолчанию (`-O0 -g`).
Для сборки с оптимизацией:

```bash
make clean
make CFLAGS="-std=c17 -Wall -Wextra -Wpedantic -O2"
./vpd_lab2
```

### 4.3. Вывод в файл

Прогресс-бар использует `\r`, поэтому при перенаправлении в файл строки
выглядят «склеенными». Для читаемого лога преобразуйте возврат каретки в
перевод строки:

```bash
make -B ITEMS=200000 ROUNDS=16 run | tr '\r' '\n' > run.txt
```

### 4.4. Очистка

```bash
make clean
```

Удаляет `obj/`, `build/`, `artifacts/`, `vpd_lab2`, `vpd_lab2_O0` и
`vpd_lab2_O2`. Каталог `experiments/out/` этой целью не удаляется — для него
есть отдельная команда:

```bash
make -C experiments clean
```

Всё перечисленное — генерируемые артефакты, они исключены из Git через
`.gitignore`. Файл `lab1/laba1_all.txt` отслеживается Git и командой
`make clean` не удаляется.

### 4.5. Справка

```bash
make help
```

## 5. Сравнение `-O0` и `-O2`

```bash
make bench        # собрать vpd_lab2_O0 и vpd_lab2_O2
make bench-run    # собрать и запустить оба варианта
```

Объектные файлы складываются в разные каталоги (`obj/o0/` и `obj/o2/`), чтобы
варианты не перезаписывали друг друга. `bench-run` выполняет две полные
нагрузки, поэтому занимает примерно вдвое больше времени, чем `make run`.

Время и `checksum` считаются по одной и той же нагрузке: `checksum` должен
совпадать, а время на `-O2` обычно меньше. На macOS `clock()` измеряет
процессорное время, а `gettimeofday()` — реальное (настенное) время, поэтому
эти два значения могут немного расходиться.

## 6. Ручная сборка

```bash
make manual
./artifacts/manual/vpd_lab2
```

Цель проходит весь конвейер и складывает результат в `artifacts/manual/`:

```text
main.c -> main.i          # препроцессор, с line markers
main.c -> main_no_lines.i # препроцессор, -P без line markers
main.c -> main.s          # ассемблер
main.c -> main.o          # объектный файл
device.c -> device.o
timer.c -> timer.o
main.o + device.o + timer.o -> vpd_lab2
```

Тот же сценарий можно выполнить скриптом:

```bash
./scripts/build_steps.sh
```

Скрипт сам выбирает `gcc-16`, если он есть, иначе `gcc`. Принудительно задать
компилятор:

```bash
CC=clang ./scripts/build_steps.sh
```

Каталог `artifacts/` удаляется командой `make clean`.

## 7. Эксперименты

```bash
make experiments
```

Цель собирает все небольшие программы в `experiments/out/` и запускает
неинтерактивные демонстрации. Отдельные команды:

```bash
make -C experiments all          # только собрать
make -C experiments run          # запустить основные демонстрации
make -C experiments run-io       # корректный и некорректный scanf
make -C experiments run-ub       # демонстрации undefined behavior
make -C experiments link-error   # ожидаемая ошибка линковки
make -C experiments keyword-error # ожидаемая ошибка компилятора
make -C experiments clean        # удалить experiments/out/
```

Особенности:

- `run-io` сам подаёт входные данные через `printf`, участие пользователя не
  требуется; некорректный ввод завершает программу с кодом `1`.
- `run-ub` содержит намеренное неопределённое поведение. Его вывод — только
  наблюдение, а не переносимый результат.
- `link-error` и `keyword-error` **должны** завершаться ошибкой. В Makefile
  команды помечены префиксом `-`, поэтому `make` продолжает работу и
  возвращает успех; в логе будет `Error 1 (ignored)`.
- На macOS используется `g++-16` для `foreach.cpp`, если он установлен;
  иначе — `c++` (Apple Clang).

## 8. Стандарты C

```bash
make standards
```

Скрипт `scripts/standards_experiments.sh` собирает одни и те же пробы в
режимах C90, C99, C11, C17 и C23 и печатает матрицу `PASS`/`FAIL`. Логи и
бинарники складываются в `artifacts/standards/`.

Ожидаемая картина при GCC 16:

```text
c99_features        c90  FAIL
c99_features        c99  PASS
c11_static_assert   c99  FAIL
c11_static_assert   c11  PASS
c23_binary          c17  FAIL
c23_binary          c23  PASS
c23_auto            c17  FAIL
c23_auto            c23  PASS
```

`FAIL` для старых стандартов — это и есть проверяемое свойство, а не поломка
проекта.

Скрипт выбирает `CC` в таком порядке: переменная окружения `CC`, затем
`gcc-16`, затем `gcc`. Поэтому `make standards` использует GNU GCC, если он
установлен. Сравнить с Apple Clang:

```bash
CC=/usr/bin/cc make standards
```

Apple Clang 16 не принимает `-std=c23`, поэтому строки C23 будут `FAIL`, а
скрипт выведет предупреждение. Чтобы явно выбрать компилятор:

```bash
CC=gcc-16 make standards
```

Или передать полный путь, если версия формулы отличается:

```bash
CC="$(brew --prefix gcc)/bin/gcc-17" make standards
```

## 9. CMake

```bash
cmake -S . -B build
cmake --build build
./build/vpd_lab2
```

Без дополнительных параметров CMake использует генератор `Unix Makefiles` и
системный `/usr/bin/cc` (Apple Clang). `CMakeLists.txt` требует C17, поэтому
эта сборка работает и без GNU GCC. Результат должен совпадать с Make-сборкой,
включая `checksum`.

Сборка через Ninja:

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/vpd_lab2
```

Если каталог `build/` уже создан с другим генератором, CMake завершится
ошибкой. Удалите каталог и настройте заново:

```bash
rm -rf build
cmake -S . -B build -G Ninja
cmake --build build
```

CMake собирает только основной проект. Эксперименты, `make standards` и
`make lab1` запускаются отдельно. Артефакты CMake (`build/`) исключены из Git;
`make clean` также удаляет этот каталог.

## 10. Makefile для лабораторной № 1

Цель `lab1` собирает вывод соседнего проекта `VvPD-Lab1`:

```bash
make lab1
```

По умолчанию используется путь `../../VvPD-Lab1` относительно `lab1/`, то
есть `VvPD-Lab1` должен лежать рядом с `VvPD-Lab2` в каталоге `VvPD-Labs`.
Другой путь задаётся переменной `LAB1`:

```bash
make LAB1=/path/to/VvPD-Lab1 lab1
```

Если файл `lab1/laba1_all.txt` уже существует, `make` может ответить
`is up to date`. Принудительно пересобрать:

```bash
make -B lab1
```

Прямой запуск с перенаправлением:

```bash
make -C lab1 run-all > lab1/laba1_all.txt 2>&1
```

Для этой цели нужны все инструменты Lab 1: Python, Node.js, TypeScript, JDK,
.NET, Go, Rust, GNU Bash и SWI-Prolog. Отсутствие отдельного инструмента не
останавливает агрегатор: команды помечены `-`, поэтому сборка продолжится, а
ошибки при цели `laba1_all.txt` записываются в файл вместе с обычным выводом
благодаря `2>&1`.

`lab1/laba1_all.txt` отслеживается Git. Если пересборка не нужна, верните
файл:

```bash
git restore lab1/laba1_all.txt
```

## 11. Особенности macOS

1. **`gcc` — это Clang.** `/usr/bin/gcc` и `/usr/bin/cc` указывают на Apple
   Clang. GNU GCC из Homebrew называется `gcc-16` (или `gcc-17` и т. д.),
   поэтому `Makefile` ищет его явно и использует, если он есть в `PATH`.
2. **Версия формулы GCC меняется.** Сейчас `brew install gcc` ставит GCC 16,
   но после обновления Homebrew бинарник может называться иначе. Проверяйте
   `ls "$(brew --prefix gcc)/bin"/gcc-*` и при необходимости передавайте
   `CC=...`.
3. **C23 требует GNU GCC.** Apple Clang 16 не принимает `-std=c23`, поэтому
   `make standards` с `/usr/bin/cc` показывает `FAIL` для C23. Это ограничение
   компилятора, а не Makefile.
4. **GNU Make 3.81.** Поставляется с Command Line Tools; возможностей GNU Make
   4.x проект не использует. Рецепты совместимы с `/bin/sh` (Bash 3.2),
   поэтому отдельный Homebrew Bash не нужен.
5. **Таймеры.** `timer.c` использует `time()`, `clock()` и `gettimeofday()`.
   На macOS `gettimeofday()` доступен без дополнительных feature-макросов;
   на Windows используется `QueryPerformanceCounter`.
6. **Архитектура.** Сборка нативная: на Apple Silicon получается arm64, на
   Intel — x86_64. `sizeof(void*)` на arm64 равен 8. Переносить бинарники
   между архитектурами без пересборки нельзя.
7. **Прогресс-бар.** `\r` и `fflush(stdout)` корректно работают в Terminal.app
   и iTerm2, но при перенаправлении в файл дают длинные строки. Используйте
   `tr '\r' '\n'` или меньшие `ITEMS`/`ROUNDS`.
8. **Артефакты.** `build/`, `obj/`, `artifacts/`, `experiments/out/`,
   `vpd_lab2`, `vpd_lab2_O0`, `vpd_lab2_O2` исключены из Git. Не коммитьте их
   после сборки.
9. **Пути Homebrew.** На Apple Silicon префикс — `/opt/homebrew`, на Intel —
   `/usr/local`. Используйте `$(brew --prefix ...)`, как в примерах выше.

## 12. Частые ошибки

| Симптом | Причина | Решение |
| --- | --- | --- |
| `xcrun: error: invalid active developer path` | не установлены или сломаны Command Line Tools | `xcode-select --install`; при необходимости `sudo xcode-select -s /Library/Developer/CommandLineTools` |
| `make: command not found` | нет Command Line Tools | `xcode-select --install` |
| `fatal error: 'stdio.h' file not found` | повреждены заголовки CLT | переустановите Command Line Tools |
| `gcc-16: command not found` | GNU GCC не установлен или формула обновилась | `brew install gcc`; проверьте `ls "$(brew --prefix gcc)/bin"/gcc-*` и задайте `make CC=...` |
| `make: Nothing to be done for 'all'` | бинарник уже собран и исходники не менялись | это нормально; для пересборки `make clean && make` или `make -B` |
| `make standards` показывает `FAIL` для C23 | используется Apple Clang | установите GCC и запустите `CC=gcc-16 make standards` |
| `warning: /usr/bin/cc does not accept -std=c23` | явно выбран Apple Clang | используйте `CC=gcc-16` или другой GNU GCC |
| `CMake Error: generator ... does not match` | `build/` создан другим генератором | `rm -rf build` и настройте заново |
| `cmake: command not found` | CMake не установлен | `brew install cmake` |
| `ninja: command not found` | Ninja не установлен | `brew install ninja` |
| `make lab1` завершается с ошибками | нет соседнего `VvPD-Lab1` или инструментов Lab 1 | задайте `LAB1=/path/to/VvPD-Lab1`, установите инструменты; ошибки в `laba1_all.txt` ожидаемы при отсутствии тулчейна |
| `link-error` / `keyword-error`: `Error 1 (ignored)` | это намеренно падающие демонстрации | не исправляйте; так и задумано |
| `make run` идёт слишком долго | большая нагрузка по умолчанию | `make -B ITEMS=1000000 ROUNDS=32 run` |
| Лог состоит из «склеенных» строк | прогресс-бар использует `\r` | `make run | tr '\r' '\n' > run.txt` |
| `zsh: permission denied: ./scripts/...` | потерян бит выполнения | `chmod +x scripts/*.sh` или запускайте `bash scripts/...` |

## 13. Полный цикл с нуля

```bash
# 1. Инструменты
xcode-select --install
brew install gcc cmake ninja

# 2. Проверка версий
clang --version
gcc-16 --version
make --version
cmake --version
ninja --version

# 3. Основной проект
make
make run

# 4. Сравнение оптимизаций
make bench-run

# 5. Ручной конвейер и эксперименты
make manual
make experiments

# 6. Матрица стандартов C
make standards

# 7. CMake-сборка
cmake -S . -B build
cmake --build build
./build/vpd_lab2

# 8. Лабораторная № 1 (если рядом лежит VvPD-Lab1 и стоят все инструменты)
make lab1

# 9. Очистка (при необходимости)
make clean
make -C experiments clean
```

## 14. Что читать дальше

- [README.md](README.md) — структура проекта и краткое описание всех целей.
- [REPORT.md](REPORT.md) — подробный отчёт с результатами экспериментов,
  включая сборку Make и CMake.
- `VPD_02.pdf` — исходное задание. Открыть:
  ```bash
  open VPD_02.pdf
  ```
