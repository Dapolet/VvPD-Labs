#!/usr/bin/env python3
"""Лабораторная работа № 3, пп. 2.6, 2.7, 2.9, 2.10.

Эксперименты с кодировками:
  * UTF-8 против UTF-32 LE/BE и BOM;
  * один байт 0xB8 в Windows-1251 и UTF-8;
  * визуально одинаковые символы (гомоглифы);
  * Unicode-идентификаторы в Python.
"""

import codecs
import sys
import unicodedata

SAMPLES = ["Hello", "Привет", "ёж", "大学", "     "]


def section(title: str) -> None:
    print()
    print("=" * 70)
    print(title)
    print("=" * 70)


def show_utf8_vs_utf32() -> None:
    section("2.6. UTF-8 против UTF-32 LE/BE")

    print(f"{'text':<10} {'encoding':<10} {'bytes':>6}  hex")
    for text in SAMPLES:
        utf8 = text.encode("utf-8")
        utf32le = text.encode("utf-32-le")
        utf32be = text.encode("utf-32-be")

        print(f"{text!r:<10} {'UTF-8':<10} {len(utf8):>6}  {utf8.hex(' ')}")
        print(f"{'':<10} {'UTF-32 LE':<10} {len(utf32le):>6}  {utf32le.hex(' ')}")
        print(f"{'':<10} {'UTF-32 BE':<10} {len(utf32be):>6}  {utf32be.hex(' ')}")
        print()

    print("Размер UTF-8 = сумма длин кодовых единиц 1..4 байта.")
    print("Размер UTF-32 = 4 * число кодовых точек (фиксированная ширина).")
    print("UTF-32 не экономичен для ASCII и кириллицы, но даёт прямой доступ")
    print("к кодовой точке по индексу.")


def show_bom() -> None:
    section("2.6. BOM и порядок байтов UTF-32")

    text = "大学"
    native = text.encode("utf-32")
    le = text.encode("utf-32-le")
    be = text.encode("utf-32-be")

    print("utf-32    :", native.hex(" "))
    print("utf-32-le :", le.hex(" "))
    print("utf-32-be :", be.hex(" "))
    print("native byte order:", sys.byteorder)

    if native.startswith(codecs.BOM_UTF32_LE):
        print("BOM: yes, FF FE 00 00 (UTF-32 LE)")
    elif native.startswith(codecs.BOM_UTF32_BE):
        print("BOM: yes, 00 00 FE FF (UTF-32 BE)")
    else:
        print("BOM: no")

    print("BOM (U+FEFF) сообщает потребителю порядок байтов и то, что текст")
    print("действительно закодирован в UTF-32. В UTF-8 BOM не нужен:")
    print("порядок байтов там фиксирован, а BOM лишь мешает (EF BB BF).")


def show_0xb8() -> None:
    section("2.7. Один байт 0xB8: Windows-1251 против UTF-8")

    data = bytes([0xB8])

    print("byte:", data.hex(" ").upper())
    print("cp1251:", data.decode("cp1251"))
    try:
        print("utf-8:", data.decode("utf-8"))
    except UnicodeDecodeError as error:
        print("UTF-8 error:", error)

    print()
    print("Буква «ё» в разных представлениях:")
    print("cp1251    :", "ё".encode("cp1251").hex(" ").upper())
    print("utf-8     :", "ё".encode("utf-8").hex(" ").upper())
    print("utf-32-le :", "ё".encode("utf-32-le").hex(" ").upper())
    print("code point: U+%04X" % ord("ё"))
    print()
    print("0xB8 — самостоятельный символ только в однобайтовой cp1251.")
    print("В UTF-8 байт 0xB8 — продолжающий (10xxxxxx), поэтому одиночный")
    print("0xB8 не является корректной UTF-8 последовательностью.")


def show_homoglyphs() -> None:
    section("2.9. Гомоглифы: кириллица против латиницы")

    cyrillic = "М3О-304БВ-26"
    latin = "M3O-304БВ-26"

    for label, value in (("кириллица", cyrillic), ("латиница ", latin)):
        chars = []
        for ch in value[:3]:
            try:
                name = unicodedata.name(ch)
            except ValueError:
                name = "<no name>"
            chars.append(f"{ch}=U+{ord(ch):04X} ({name})")
        print(label, "|", " ".join(chars))
        print(" " * len(label), "| UTF-8:", value.encode("utf-8").hex(" ").upper())

    print()
    print("Строки равны?", cyrillic == latin)
    print("Длина в байтах:", len(cyrillic.encode("utf-8")), "и", len(latin.encode("utf-8")))
    print("NFKC делает их равными?", unicodedata.normalize("NFKC", cyrillic) == latin)
    print()
    print("NFKC не смешивает разные алфавиты: визуальная похожесть не означает")
    print("одинаковость. Для валидации формата группы нужен явный шаблон")
    print("с кириллическими М и О, иначе строка с латинскими M/O пройдёт")
    print("проверку там, где не должна.")


def show_identifiers() -> None:
    section("2.10. Unicode-идентификаторы в Python")

    студент = 304
    студенты = 30
    группа = "М3О-119БВ-26"
    学生 = 7
    大学 = "университет"

    for name in ("студент", "студенты", "группа", "学生", "大学"):
        print(f"{name}: {locals()[name]!r}")

    print()
    print("Python 3 разрешает буквы Unicode в идентификаторах (PEP 3131).")
    print("Технически это работает, но в командном проекте без правил")
    print("нейминга лучше придерживаться одного естественного языка")
    print("и ASCII-имён: инструменты, grep, шрифты и коллеги скажут спасибо.")


def show_utf16_and_graphemes() -> None:
    section("Дополнительно: UTF-16 и графемные кластеры")

    print(f"{'text':<10} {'UTF-16 LE':>12} {'UTF-16 BE':>10} {'code points':>12}")
    for text in SAMPLES:
        le = text.encode("utf-16-le")
        be = text.encode("utf-16-be")
        print(f"{text!r:<10} {len(le):>12} {len(be):>10} {len(text):>12}")
    print("UTF-16: 2 байта на кодовую единицу; символы вне BMP — суррогатная пара.")

    print()
    composed = "\u0439"          # й — одна кодовая точка
    decomposed = "\u0438\u0306"  # и + комбинируемая breve
    family = "\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466"

    def approximate_graphemes(value: str) -> int:
        count = 0
        for ch in value:
            if (
                unicodedata.combining(ch)
                or ch == "\u200d"
                or 0xFE00 <= ord(ch) <= 0xFE0F
                or 0x1F3FB <= ord(ch) <= 0x1F3FF
            ):
                continue
            count += 1
        return count

    for label, value in (
        ("составная й", composed),
        ("и + U+0306", decomposed),
        ("семья эмодзи", family),
    ):
        print(
            f"{label:<14} utf-8={len(value.encode('utf-8')):>3} bytes, "
            f"code points={len(value):>2}, "
            f"approx graphemes={approximate_graphemes(value)}"
        )

    print()
    print("Графемный кластер — то, что пользователь воспринимает как один символ.")
    print("В стандартной библиотеке Python нет \\X (Unicode графемы), поэтому")
    print("выше приведена приближённая оценка; точный разбор требует библиотеки")
    print("(например, regex с \\X) или ICU.")


def main() -> None:
    show_utf8_vs_utf32()
    show_bom()
    show_0xb8()
    show_homoglyphs()
    show_identifiers()
    show_utf16_and_graphemes()


if __name__ == "__main__":
    main()
