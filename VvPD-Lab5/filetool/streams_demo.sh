#!/usr/bin/env bash
#
# Демонстрация stdin/stdout/stderr, перенаправлений >, >>, 2>, 2>&1 и |.
# Запуск: bash filetool/streams_demo.sh [output-dir]

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

OUT="${1:-artifacts/streams}"
rm -rf "$OUT"
mkdir -p "$OUT"

printf 'alpha\nbeta\ngamma\n' >"$OUT/input.txt"

# 1. Обычный stdout и отдельный stderr: > и 2>
./filetool/filetool --input-file "$OUT/input.txt" \
    --output-file "$OUT/copy.txt" --show-stats \
    >"$OUT/stdout.txt" 2>"$OUT/stderr.txt"

# 2. >> добавляет строку в конец, а не перезаписывает файл.
./filetool/filetool --input-file "$OUT/input.txt" \
    --output-file "$OUT/copy2.txt" --show-stats \
    >>"$OUT/stdout.txt" 2>>"$OUT/stderr.txt"

# 3. 2> перехватывает только диагностику; stdout остаётся пустым.
set +e
./filetool/filetool --vot-takogo-vida \
    >"$OUT/unknown_stdout.txt" 2>"$OUT/errors.txt"
UNKNOWN_CODE=$?
set -e

# 4. 2>&1 объединяет оба потока в один файл.
set +e
./filetool/filetool --input-file "$OUT/missing.txt" \
    --output-file "$OUT/x.txt" >"$OUT/all.txt" 2>&1
MISSING_CODE=$?
set -e

# 5. Конвейер: stdout первой программы читает вторая.
./filetool/filetool --input-file "$OUT/input.txt" \
    --output-file "$OUT/copy3.txt" --show-stats \
    | tr '[:lower:]' '[:upper:]' >"$OUT/pipe.txt"

printf '=== > and >> : stdout.txt ===\n'
cat "$OUT/stdout.txt"
printf '=== stderr.txt (expected empty) ===\n'
cat "$OUT/stderr.txt"
printf '=== 2> : unknown_stdout.txt (expected empty) ===\n'
cat "$OUT/unknown_stdout.txt"
printf '=== 2> : errors.txt ===\n'
cat "$OUT/errors.txt"
printf 'unknown option exit code: %d\n' "$UNKNOWN_CODE"
printf '=== 2>&1 : all.txt ===\n'
cat "$OUT/all.txt"
printf 'missing input exit code: %d\n' "$MISSING_CODE"
printf '=== | : pipe.txt ===\n'
cat "$OUT/pipe.txt"
printf '=== copied files are byte-identical ===\n'
cmp "$OUT/input.txt" "$OUT/copy.txt" && echo "copy.txt: identical"
cmp "$OUT/input.txt" "$OUT/copy2.txt" && echo "copy2.txt: identical"
cmp "$OUT/input.txt" "$OUT/copy3.txt" && echo "copy3.txt: identical"
