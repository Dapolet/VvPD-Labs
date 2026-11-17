#!/usr/bin/env python3
"""Лабораторная работа № 3, п. 2.8.

Base64 — кодирование произвольных байтов в текст из 64 ASCII-символов.
Это не кодировка Unicode и не замена UTF-8.
"""

import base64
import hashlib
import math
import tempfile
from pathlib import Path


def section(title: str) -> None:
    print()
    print("=" * 70)
    print(title)
    print("=" * 70)


def full_cycle() -> None:
    section("2.8. Полный цикл: Unicode → UTF-8 → Base64 → байты → UTF-8")

    text = "大学"
    utf8 = text.encode("utf-8")
    encoded = base64.b64encode(utf8)
    decoded_bytes = base64.b64decode(encoded)
    decoded_text = decoded_bytes.decode("utf-8")

    print("Text:", text)
    print("UTF-8 bytes:", utf8.hex(" ").upper())
    print("Base64:", encoded.decode("ascii"))
    print("Decoded bytes:", decoded_bytes.hex(" ").upper())
    print("Decoded text:", decoded_text)
    print("Round-trip OK:", text == decoded_text)
    print()
    print("Длина UTF-8:", len(utf8), "байт; длина Base64:", len(encoded), "символов.")
    print("Рост примерно на треть: Base64 кодирует 3 байта в 4 ASCII-символа.")


def padding_demo() -> None:
    section("Padding '=' и длина, не кратная трём")

    for size in range(0, 7):
        raw = bytes(range(65, 65 + size))
        encoded = base64.b64encode(raw).decode("ascii")
        expected = 4 * math.ceil(size / 3)

        print(
            f"n={size}: raw={raw.hex(' ').upper() or '(empty)':<14} "
            f"base64={encoded:<12} length={len(encoded):>2} "
            f"(4*ceil(n/3)={expected})"
        )

    print()
    print("'=' — padding: дополняет последнюю группу до 4 символов.")
    print("Один '=' — остаток 2 байта, два '=' — остаток 1 байт.")


def url_safe_demo() -> None:
    section("Обычный Base64 против URL-safe Base64")

    raw = bytes([251, 255, 239, 1, 2])

    normal = base64.b64encode(raw).decode("ascii")
    urlsafe = base64.urlsafe_b64encode(raw).decode("ascii")

    print("raw        :", raw.hex(" ").upper())
    print("base64     :", normal)
    print("base64url  :", urlsafe)
    print("'+' -> '-', '/' -> '_' (чтобы символы не конфликтовали с URL).")


def overhead_demo() -> None:
    section("Дополнительно: формула 4 × ceil(n / 3) и накладные расходы")

    print(f"{'n':>6} {'base64':>8} {'4*ceil(n/3)':>12} {'overhead':>10}")
    for size in (1, 2, 3, 4, 5, 10, 100, 1024, 100_000):
        expected = 4 * math.ceil(size / 3)
        overhead = (expected - size) / size * 100
        print(f"{size:>6} {expected:>8} {expected:>12} {overhead:>9.2f}%")


def binary_roundtrip() -> None:
    section("Дополнительно: бинарный файл → Base64 → файл → SHA-256")

    original = bytes((i * 37 + 11) % 256 for i in range(1024))

    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "original.bin"
        encoded_path = Path(tmp) / "encoded.txt"
        restored = Path(tmp) / "restored.bin"

        source.write_bytes(original)
        encoded_path.write_text(base64.b64encode(source.read_bytes()).decode("ascii"), encoding="ascii")
        restored.write_bytes(base64.b64decode(encoded_path.read_text(encoding="ascii")))

        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
        restored_hash = hashlib.sha256(restored.read_bytes()).hexdigest()

        print("source size:", source.stat().st_size, "bytes")
        print("source sha256  :", source_hash)
        print("restored sha256:", restored_hash)
        print("byte-identical:", source.read_bytes() == restored.read_bytes())

    print()
    print("Base64 обратим без потерь; он не сжимает и не шифрует данные.")


def main() -> None:
    full_cycle()
    padding_demo()
    url_safe_demo()
    overhead_demo()
    binary_roundtrip()


if __name__ == "__main__":
    main()
