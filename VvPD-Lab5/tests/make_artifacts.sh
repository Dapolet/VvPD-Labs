#!/usr/bin/env bash
#
# Генерирует artifacts/ для отчёта VPD Lab 05.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

ART="${1:-artifacts}"
mkdir -p "$ART"

rm -f "$ART"/[0-9][0-9]_*.txt
rm -f "$ART"/students.dat "$ART"/students.dat.tmp "$ART"/students.dat.journal \
      "$ART"/imported.dat "$ART"/imported.dat.tmp "$ART"/imported.dat.journal \
      "$ART"/session.dat "$ART"/session.dat.tmp "$ART"/session.dat.journal \
      "$ART"/copy.txt "$ART"/export.json "$ART"/bad_magic.dat \
      "$ART"/bad_checksum.dat "$ART"/truncated.dat "$ART"/bad.json \
      "$ART"/bad_fk.json "$ART"/test.dat "$ART"/test.dat.tmp \
      "$ART"/test.dat.journal "$ART"/test_export.json \
      "$ART"/bad_syntax.json "$ART"/trunc.json
rm -rf "$ART/streams"

capture() {
    local file="$1"
    shift
    {
        printf '$'
        printf ' %q' "$@"
        printf '\n'
        set +e
        "$@"
        local code=$?
        set -e
        printf 'exit=%d\n' "$code"
    } >>"$file" 2>&1
}

# ---------------------------------------------------------------------------
# 01. main(void) and main(argc, argv)
# ---------------------------------------------------------------------------
{
    printf '$ ./filetool/argv_demo alpha "beta gamma"\n'
    ./filetool/argv_demo alpha "beta gamma"
    printf '\n$ ./filetool/argv_void_demo\n'
    ./filetool/argv_void_demo
    printf '\n$ ./database/vpddb --show-args\n'
    ./database/vpddb --show-args
} >"$ART/01_argv.txt" 2>&1

# ---------------------------------------------------------------------------
# 02. filetool
# ---------------------------------------------------------------------------
{
    printf '$ ./filetool/filetool --input-file tests/sample_input.txt '
    printf -- '--output-file %s/copy.txt --show-stats\n' "$ART"
    ./filetool/filetool --input-file tests/sample_input.txt \
        --output-file "$ART/copy.txt" --show-stats
    if cmp -s tests/sample_input.txt "$ART/copy.txt"; then
        printf 'cmp: output is byte-identical to input\n'
    else
        printf 'cmp: output differs\n'
    fi
} >"$ART/02_filetool.txt" 2>&1

# ---------------------------------------------------------------------------
# 03. streams and pipe
# ---------------------------------------------------------------------------
bash filetool/streams_demo.sh "$ART/streams" >"$ART/03_streams.txt" 2>&1

# ---------------------------------------------------------------------------
# 04-08. Database, CRUD, stats, check
# ---------------------------------------------------------------------------
{
    printf '$ ./database/vpddb --database %s/students.dat --init --seed --show-stats\n' "$ART"
    ./database/vpddb --database "$ART/students.dat" --init --seed --show-stats
} >"$ART/04_init_seed.txt" 2>&1

{
    printf '$ ./database/vpddb --database %s/students.dat --file tests/demo_session.txt\n' "$ART"
    ./database/vpddb --database "$ART/students.dat" \
        --file tests/demo_session.txt
} >"$ART/05_session.txt" 2>&1

{
    printf "$ ./database/vpddb --database %s/students.dat --query 'SELECT students ORDER BY id'\n" "$ART"
    ./database/vpddb --database "$ART/students.dat" \
        --query 'SELECT students ORDER BY id'
} >"$ART/06_select.txt" 2>&1

{
    printf '$ ./database/vpddb --database %s/students.dat --show-stats\n' "$ART"
    ./database/vpddb --database "$ART/students.dat" --show-stats
} >"$ART/07_stats.txt" 2>&1

{
    printf '$ ./database/vpddb --database %s/students.dat --check\n' "$ART"
    ./database/vpddb --database "$ART/students.dat" --check
} >"$ART/08_check.txt" 2>&1

# ---------------------------------------------------------------------------
# 09-12. JSON export/import and statham.json
# ---------------------------------------------------------------------------
{
    printf '$ ./database/vpddb --database %s/students.dat --export json/export.json\n' "$ART"
    ./database/vpddb --database "$ART/students.dat" --export json/export.json
    cp json/export.json "$ART/export.json"
} >"$ART/09_export.txt" 2>&1

{
    printf '$ ./database/vpddb --database %s/imported.dat --init --import %s/export.json --show-stats\n' "$ART" "$ART"
    ./database/vpddb --database "$ART/imported.dat" --init \
        --import "$ART/export.json" --show-stats
} >"$ART/10_import.txt" 2>&1

./json/json_demo json/statham.json >"$ART/11_json_statham.txt" 2>&1
./json/json_demo json/export.json >"$ART/12_json_export.txt" 2>&1

# ---------------------------------------------------------------------------
# 13. Errors
# ---------------------------------------------------------------------------
ERRORS="$ART/13_errors.txt"
: >"$ERRORS"
capture "$ERRORS" ./database/vpddb --database "$ART/students.dat" \
    --query 'INSERT student 301 "Duplicate" 11'
capture "$ERRORS" ./database/vpddb --database "$ART/students.dat" \
    --query 'INSERT student 402 "Bad FK" 999'
capture "$ERRORS" ./database/vpddb --database "$ART/students.dat" \
    --query 'DELETE groups WHERE id = 11'
capture "$ERRORS" ./database/vpddb --database "$ART/students.dat" \
    --query 'SELECT students WHERE faculty_id = 1'
capture "$ERRORS" ./database/vpddb --database "$ART/students.dat" \
    --query 'SELECT teachers'
capture "$ERRORS" ./database/vpddb --database "$ART/students.dat" \
    --query 'SELECT students WHERE'
capture "$ERRORS" ./database/vpddb --database "$ART/students.dat" \
    --query 'UPDATE students SET id = 9 WHERE id = 301'
capture "$ERRORS" ./database/vpddb --database "$ART/students.dat" \
    --query 'DELETE students'
printf '%s' '{ "students": [ { "id": 1, "name": "X", } ] }' >"$ART/bad.json"
capture "$ERRORS" ./database/vpddb --database "$ART/imported.dat" \
    --import "$ART/bad.json"
printf '%s' '{ "students": [ { "id": 1, "name": "X", "group_id": 999 } ] }' >"$ART/bad_fk.json"
capture "$ERRORS" ./database/vpddb --database "$ART/imported.dat" \
    --import "$ART/bad_fk.json"
capture "$ERRORS" ./database/vpddb --database "$ART/missing.dat" --show-stats

# ---------------------------------------------------------------------------
# 14. Corrupted storage
# ---------------------------------------------------------------------------
CORRUPT="$ART/14_corruption.txt"
: >"$CORRUPT"

cp "$ART/students.dat" "$ART/bad_magic.dat"
printf 'BADDATA!' | dd of="$ART/bad_magic.dat" bs=1 seek=0 \
    conv=notrunc status=none
capture "$CORRUPT" ./database/vpddb --database "$ART/bad_magic.dat" --show-stats

cp "$ART/students.dat" "$ART/bad_checksum.dat"
printf 'X' | dd of="$ART/bad_checksum.dat" bs=1 seek=52 \
    conv=notrunc status=none
capture "$CORRUPT" ./database/vpddb --database "$ART/bad_checksum.dat" --show-stats

head -c 20 "$ART/students.dat" >"$ART/truncated.dat"
capture "$CORRUPT" ./database/vpddb --database "$ART/truncated.dat" --show-stats

# ---------------------------------------------------------------------------
# 15-16. Config and pipeline
# ---------------------------------------------------------------------------
{
    printf '$ ./database/vpddb --config config.yaml --database %s/students.dat --query "SELECT faculties"\n' "$ART"
    ./database/vpddb --config config.yaml --database "$ART/students.dat" \
        --query 'SELECT faculties'
} >"$ART/15_config.txt" 2>&1

{
    printf '%s\n' "$ printf 'SELECT students WHERE id = 304\\n' | ./database/vpddb --database $ART/students.dat --query -"
    printf 'SELECT students WHERE id = 304\n' | \
        ./database/vpddb --database "$ART/students.dat" --query -
} >"$ART/16_pipeline.txt" 2>&1

printf 'artifacts written to %s\n' "$ART"
ls -1 "$ART" | sed 's/^/  /'
