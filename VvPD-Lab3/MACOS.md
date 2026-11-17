# Сборка и запуск на macOS

Документ описывает подготовку окружения и команды для лабораторной работы № 3
на macOS. Все команды выполняются из **корня репозитория** (каталог, где лежат
`Makefile` и этот файл), если не указано иное.

Проверено на macOS 15.8.1 (Apple Silicon, arm64) с Apple Clang 16.0.0,
GNU Make 3.81, Python 3.15 и Node.js 24. Версии могут быть новее — команды не
меняются.

## 1. Что понадобится

| Компонент | Зачем | Откуда |
| --- | --- | --- |
| Xcode Command Line Tools | `clang`, `make`, системные заголовки | Apple |
| Python 3 | `encoding.py`, `base64_demo.py`, `regex.py`, локальный веб-сервер | Homebrew (рекомендуется) |
| Node.js | `web/selftest.js`, проверка `validation.js` | Homebrew |

Отдельный GCC не нужен: на macOS команда `gcc` — это Apple Clang. Node.js
необязателен: без него `make test` пропускает веб-проверку с сообщением.

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

### 2.3. Python и Node.js

```bash
brew install python node
```

Проверка:

```bash
python3 --version
node --version
```

Если `python3` указывает на старую системную версию, проверьте пути:

```bash
command -v python3
python3 --version
```

Homebrew-версия обычно находится в `/opt/homebrew/bin/python3` (Apple Silicon)
или `/usr/local/bin/python3` (Intel).

## 3. Быстрый старт

```bash
make            # собрать C-программы и генератор
make test       # запустить все демонстрации и web/selftest.js
make regex      # пересоздать VPD_Lab3.txt и выполнить regex.py
```

Запуск веб-части:

```bash
python3 -m http.server 8000
# затем откройте http://localhost:8000/web/
```

Остановить сервер — `Ctrl+C`.

## 4. Подробно по целям Makefile

### 4.1. Сборка

```bash
make
```

Цель `all` собирает четыре C-программы из `strings/` и генератор из `regex/`.
По умолчанию используется `cc` (Apple Clang) и флаги
`-std=c17 -Wall -Wextra -Wpedantic -O2`. Чтобы указать компилятор явно:

```bash
make CC=clang
```

Собрать только C-эксперименты (без генератора):

```bash
make strings
```

Собрать только генератор:

```bash
make generator
```

Ручная сборка одного файла (например, если нужно обойти проблему с
Unicode-идентификаторами):

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O2 strings/ascii.c -o strings/ascii
```

Сборка с отладочной информацией вместо оптимизации:

```bash
make clean
make CFLAGS="-std=c17 -Wall -Wextra -Wpedantic -O0 -g"
```

### 4.2. C-программы

```bash
./strings/ascii
./strings/utf8
./strings/types
./strings/unicode_identifiers
```

Все четыре программы самодостаточны и не читают аргументы командной строки.
Кириллица выводится в UTF-8 — Terminal.app и iTerm2 на macOS настроены так по
умолчанию.

`strings/unicode_identifiers.c` — дополнительный эксперимент: в исходнике
используются идентификаторы на кириллице и китайском. Он собирается Apple
Clang 16. Если на старой версии компилятора этот файл не собирается,
скомпилируйте остальные три отдельно:

```bash
for src in ascii utf8 types; do
  cc -std=c17 -Wall -Wextra -Wpedantic -O2 "strings/$src.c" -o "strings/$src"
done
```

### 4.3. Python-демонстрации

```bash
python3 strings/encoding.py
python3 strings/base64_demo.py
```

`encoding.py` показывает UTF-8/UTF-32, BOM, cp1251 и гомоглифы;
`base64_demo.py` — Base64, padding, Base64URL и обратимость. Оба скрипта
ничего не изменяют на диске, кроме временных данных в памяти.

### 4.4. Генератор и регулярные выражения

```bash
cd regex
./generator > VPD_Lab3.txt
python3 regex.py
cd ..
```

`generator` пишет в stdout только строку данных, а статистику — в stderr,
поэтому перенаправление `>` не портит файл.

Важно: `make regex` и команды выше **перезаписывают отслеживаемые Git файлы**
`regex/VPD_Lab3.txt` и `regex/VPD_Lab3_final.txt`. Данные генерируются
случайно, поэтому количества вхождений будут отличаться от чисел в
`REPORT.md`. Если изменения не нужны, верните файлы:

```bash
git restore regex/VPD_Lab3.txt regex/VPD_Lab3_final.txt
```

### 4.5. Веб-форма

Веб-часть не требует сборки. Цель `make web` только напоминает команды:

```bash
make web
```

Два способа открыть страницу:

```bash
# Вариант 1: локальный сервер (рекомендуется)
python3 -m http.server 8000
open http://localhost:8000/web/

# Вариант 2: открыть файл напрямую в браузере
open web/index.html
```

Сервер должен запускаться из корня репозитория: путь `/web/` отсчитывается от
корня. Остановка — `Ctrl+C` в терминале с сервером.

Проверка JavaScript без браузера:

```bash
node --check web/validation.js
node web/selftest.js
```

Ожидаемый результат — список строк `PASS` и `All tests passed`.

### 4.6. Полная проверка

```bash
make test
```

Цель последовательно запускает:

1. `strings/ascii`, `strings/utf8`, `strings/types`,
   `strings/unicode_identifiers`;
2. `strings/encoding.py`, `strings/base64_demo.py`;
3. `node --check web/validation.js` и `node web/selftest.js`, если Node
   установлен.

Если Node не найден, шаг 3 пропускается с сообщением
`node not found: web/selftest.js skipped` — это не ошибка.

### 4.7. Очистка

```bash
make clean
```

Удаляет собранные бинарники и каталоги `__pycache__`. Исходники, PDF и
сгенерированные `.txt` не затрагиваются. Полный сброс рабочей копии:

```bash
git restore regex/VPD_Lab3.txt regex/VPD_Lab3_final.txt
make clean
```

## 5. Особенности macOS

1. **`gcc` — это Clang.** В `/usr/bin/gcc` находится обёртка Apple Clang,
   поэтому `make` без параметров работает. Для наглядности можно явно указать
   `make CC=clang`.
2. **GNU Make 3.81.** Поставляется с Command Line Tools. `Makefile` не
   использует возможностей GNU Make 4.x, поэтому дополнительно ставить
   `brew install make` не нужно.
3. **Локаль.** `strings/types.c` вызывает `setlocale("")`; если в терминале
   локаль не задана, программа сама пробует `en_US.UTF-8`, `C.UTF-8` и
   `UTF-8`. При проблемах с выводом wide-строк задайте локаль вручную:
   ```bash
   export LANG=en_US.UTF-8
   export LC_ALL=en_US.UTF-8
   ```
4. **Размеры типов зависят от платформы.** На macOS `sizeof(wchar_t) = 4`.
   Значение `sizeof(uint_fast32_t)` может отличаться от Linux: на Apple
   Silicon/Clang 16 оно равно 4, в отчёте для Linux — 8. Это нормально,
   ориентируйтесь на фактический вывод `./strings/types`.
5. **Unicode-идентификаторы.** Исходники в UTF-8, современный Apple Clang
   (15+) компилирует `strings/unicode_identifiers.c` без дополнительных
   флагов. На старом компиляторе собирайте только обязательные файлы (см.
   раздел 4.2).
6. **Случайный генератор.** `VPD_Lab3.txt` каждый раз новый, поэтому числа в
   выводе `regex.py` не совпадают с отчётом. Это ожидаемо.
7. **Артефакты.** Бинарники `strings/ascii`, `strings/utf8`, `strings/types`,
   `strings/unicode_identifiers`, `regex/generator`, а также `__pycache__/` и
   `.DS_Store` уже перечислены в `.gitignore`.

## 6. Частые ошибки

| Симптом | Причина | Решение |
| --- | --- | --- |
| `xcrun: error: invalid active developer path` | не установлены или сломаны Command Line Tools | `xcode-select --install`; при необходимости `sudo xcode-select -s /Library/Developer/CommandLineTools` |
| `make: command not found` | нет Command Line Tools | `xcode-select --install` |
| `clang: error: no such file or directory: 'strings/ascii.c'` | команда выполнена не из корня репозитория | `cd` в каталог с `Makefile` |
| `python3: command not found` | Python 3 не установлен | `brew install python` |
| `node: command not found` | Node.js не установлен | `brew install node`; либо пропустите `web/selftest.js` |
| `VPD_Lab3.txt not found; run ./generator > VPD_Lab3.txt first` | не сгенерирован входной файл | `cd regex && ./generator > VPD_Lab3.txt`, затем `python3 regex.py` |
| Ошибка компиляции `strings/unicode_identifiers.c` (неизвестные символы в идентификаторах) | старый Clang не поддерживает UTF-8-идентификаторы | соберите `ascii`, `utf8`, `types` вручную (раздел 4.2) |
| Кириллица выводится как `????` или `\uXXXX` | терминал или локаль не в UTF-8 | включите UTF-8 в терминале, задайте `LANG=en_US.UTF-8` |
| `Address already in use` при `http.server` | порт 8000 занят | `python3 -m http.server 8001`, затем `http://localhost:8001/web/` |
| `Cannot find module '.../web/selftest.js'` | команда запущена не из корня репозитория | запускайте `node web/selftest.js` из корня |
| Страница открывается, но форма не работает | открыт не тот файл или сломан `validation.js` | откройте `web/index.html` либо `http://localhost:8000/web/`; проверьте `node --check web/validation.js` |

## 7. Полный цикл с нуля

```bash
# 1. Инструменты
xcode-select --install
brew install python node

# 2. Проверка версий
clang --version
make --version
python3 --version
node --version

# 3. Сборка и проверка
make
make test

# 4. Генератор и регулярные выражения (перезаписывает .txt в regex/)
make regex

# 5. Веб-форма
python3 -m http.server 8000
# http://localhost:8000/web/

# 6. Очистка артефактов (при необходимости)
make clean
git restore regex/VPD_Lab3.txt regex/VPD_Lab3_final.txt
```

## 8. Что читать дальше

- [README.md](README.md) — структура проекта, этапы работы и ожидаемые
  результаты.
- [REPORT.md](REPORT.md) — подробный отчёт с выводом экспериментов.
- `VPD_03.pdf` — исходное задание. Открыть:
  ```bash
  open VPD_03.pdf
  ```
