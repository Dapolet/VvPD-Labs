#!/usr/bin/env python3
"""Лабораторная работа № 3, пп. 3.3–3.5.

* re.findall() — подсчёт неперекрывающихся вхождений 2^0 … 2^64;
* re.sub() — удаление степеней двойки от 2^64 к 2^0;
* сравнение обычного поиска и поиска через (?=...);
* дополнительно: re.ASCII против Unicode-режима для \\w.
"""

from pathlib import Path
import re
import sys

BASE = Path(__file__).resolve().parent
SOURCE = BASE / "VPD_Lab3.txt"
FINAL = BASE / "VPD_Lab3_final.txt"


def section(title: str) -> None:
    print()
    print("=" * 70)
    print(title)
    print("=" * 70)


def main() -> None:
    if not SOURCE.exists():
        sys.exit(f"{SOURCE.name} not found; run ./generator > VPD_Lab3.txt first")

    text = SOURCE.read_text(encoding="utf-8").strip()
    powers = [str(2**n) for n in range(65)]

    section("3.3. Вхождения степеней двойки (re.findall)")
    print(f"{'power':>7} {'value':>21} {'count':>7}")
    print("-" * 38)

    counts = {}
    total = 0

    for n, value in enumerate(powers):
        count = len(re.findall(re.escape(value), text))
        counts[value] = count
        total += count
        print(f"2^{n:<5} {value:>21} {count:>7}")

    print("-" * 38)
    print(f"{'total':>7} {'':>21} {total:>7}")

    section("3.4. Удаление степеней двойки от 2^64 к 2^0")

    result = text
    removed_total = 0

    for value in reversed(powers):
        result, removed = re.subn(re.escape(value), "", result)
        removed_total += removed
        if removed:
            print(f"removed {value:<21} {removed:>7} time(s)")

    FINAL.write_text(result, encoding="utf-8")

    source_size = SOURCE.stat().st_size
    final_size = FINAL.stat().st_size

    print()
    print(f"source file      : {SOURCE.name} — {source_size} bytes")
    print(f"result file      : {FINAL.name} — {final_size} bytes")
    print(f"bytes removed    : {source_size - final_size}")
    print(f"findall matches  : {total} (non-overlapping, initial text)")
    print(f"subn removals    : {removed_total} (matches disappear and reappear)")

    print()
    print("Почему важен порядок удаления:")
    print("Удаление короткой записи раньше длинной разрушает длинную запись.")
    print("Например, '1' (2^0) внутри '128' (2^7) превращает '128' в '28'.")

    toy = "128 64 32"
    ascending = toy
    for value in powers:  # от 2^0 к 2^64 — неправильный порядок
        ascending = re.sub(re.escape(value), "", ascending)

    descending = toy
    for value in reversed(powers):  # от 2^64 к 2^0 — правильный порядок
        descending = re.sub(re.escape(value), "", descending)

    print(f"  исходная строка        : {toy!r}")
    print(f"  от 2^0 к 2^64          : {ascending!r}")
    print(f"  от 2^64 к 2^0          : {descending!r}")

    section("3.5. Перекрывающиеся совпадения")

    pattern = re.escape("1024")
    normal = re.findall(pattern, text)
    overlapping = re.findall(rf"(?=({pattern}))", text)

    print(f"pattern '1024': normal={len(normal)}, overlapping={len(overlapping)}")
    print("У '1024' нет собственного перекрывающегося продолжения, поэтому")
    print("результаты совпали. Разницу показывает шаблон с самоперекрытием:")
    print()

    for sample_pattern in ("101", "11"):
        escaped = re.escape(sample_pattern)
        plain = re.findall(escaped, text)
        lookahead = re.findall(rf"(?=({escaped}))", text)
        print(f"pattern {sample_pattern!r}: normal={len(plain)}, overlapping={len(lookahead)}")

    print()
    print("(?=(...)) — позитивная опережающая проверка: она не потребляет")
    print("символы, поэтому следующая попытка начинается со следующей позиции.")

    section("Дополнительно: re.ASCII против Unicode-режима для \\w")

    sample = "Привет hello 123"
    unicode_words = re.findall(r"\w+", sample)
    ascii_words = re.findall(r"\w+", sample, re.ASCII)

    print("text:", sample)
    print("unicode \\w+:", unicode_words, "count:", len(unicode_words))
    print("ASCII   \\w+:", ascii_words, "count:", len(ascii_words))
    print("В Unicode-режиме кириллица входит в \\w, в ASCII-режиме — нет.")


if __name__ == "__main__":
    main()
