#!/usr/bin/env bash
#
# Регрессионные тесты VPD Lab 05.
# Каждый тест печатает PASS/FAIL; в конце выводится итог.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

TMP="$(mktemp -d "${TMPDIR:-/tmp}/vpdlab5.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

PASS=0
FAIL=0

pass() {
    PASS=$((PASS + 1))
    printf 'PASS: %s\n' "$1"
}

fail() {
    FAIL=$((FAIL + 1))
    printf 'FAIL: %s\n' "$1"
}

run_status() {
    local description="$1"
    local expected="$2"
    shift 2

    set +e
    "$@" >"$TMP/out" 2>"$TMP/err"
    local status=$?
    set -e

    if [ "$status" -eq "$expected" ]; then
        pass "$description"
    else
        fail "$description (expected exit $expected, got $status; stderr: $(tr '\n' ' ' <"$TMP/err"))"
    fi
}

assert_contains() {
    local description="$1"
    local needle="$2"
    local file="$3"
    if grep -Fq -- "$needle" "$file"; then
        pass "$description"
    else
        fail "$description (missing '$needle')"
    fi
}

assert_not_contains() {
    local description="$1"
    local needle="$2"
    local file="$3"
    if grep -Fq -- "$needle" "$file"; then
        fail "$description (unexpected '$needle')"
    else
        pass "$description"
    fi
}

assert_equals() {
    local description="$1"
    local expected="$2"
    local actual="$3"
    if [ "$actual" = "$expected" ]; then
        pass "$description"
    else
        fail "$description (expected '$expected', got '$actual')"
    fi
}

# ---------------------------------------------------------------------------
# 1–2. main(void), main(argc, argv), argc/argv
# ---------------------------------------------------------------------------

run_status "argv demo exits with 0" 0 ./filetool/argv_demo alpha "beta gamma"
assert_contains "argv demo prints argc" "argc = 3" "$TMP/out"
assert_contains "argv demo preserves argument with space" \
    'argv[2] = "beta gamma"' "$TMP/out"

run_status "main(void) demo exits with 0" 0 ./filetool/argv_void_demo
assert_contains "main(void) demo explains absence of parameters" \
    "int main(void)" "$TMP/out"

run_status "vpddb --show-args prints argv" 0 ./database/vpddb --show-args
assert_contains "vpddb --show-args prints argc" "argc =" "$TMP/out"

# ---------------------------------------------------------------------------
# 3–6. filetool, exit codes, streams and pipeline
# ---------------------------------------------------------------------------

SIZE="$(wc -c < tests/sample_input.txt | tr -d ' ')"
run_status "filetool copies a file" 0 \
    ./filetool/filetool --input-file tests/sample_input.txt \
    --output-file "$TMP/copy.txt" --show-stats
assert_contains "filetool reports copied bytes" "Copied: $SIZE bytes" "$TMP/out"
if cmp -s tests/sample_input.txt "$TMP/copy.txt"; then
    pass "filetool output is byte-identical"
else
    fail "filetool output differs from input"
fi

run_status "filetool rejects an unknown option" 64 \
    ./filetool/filetool --vot-takogo-vida
assert_contains "filetool writes unknown-option error to stderr" \
    "unknown option" "$TMP/err"
assert_equals "filetool keeps stdout clean on unknown option" "" "$(cat "$TMP/out")"

run_status "filetool reports missing input file" 66 \
    ./filetool/filetool --input-file "$TMP/missing.txt" \
    --output-file "$TMP/x.txt"

run_status "filetool reports unwritable output path" 73 \
    ./filetool/filetool --input-file tests/sample_input.txt \
    --output-file "$TMP/no/such/dir/x.txt"

printf 'VPD pipeline\n' | ./filetool/filetool --input-file /dev/stdin \
    --output-file "$TMP/pipe.txt" --show-stats | tr '[:lower:]' '[:upper:]' \
    >"$TMP/pipe.out" 2>"$TMP/pipe.err"
assert_contains "pipe passes stdout of filetool to tr" \
    "COPIED:" "$TMP/pipe.out"

# ---------------------------------------------------------------------------
# 7–12. Database, CRUD, keys, integrity, indexes
# ---------------------------------------------------------------------------

DB="$TMP/students.dat"

run_status "database --init --seed succeeds" 0 \
    ./database/vpddb --database "$DB" --init --seed --show-stats
assert_contains "seed has two faculties" "faculties: 2" "$TMP/out"
assert_contains "seed has three groups" "groups: 3" "$TMP/out"
assert_contains "seed has five students" "students: 5" "$TMP/out"

run_status "database integrity check succeeds" 0 \
    ./database/vpddb --database "$DB" --check
assert_contains "integrity check prints ok" "integrity: ok" "$TMP/out"

run_status "SELECT students returns all rows" 0 \
    ./database/vpddb --database "$DB" --query 'SELECT students'
assert_contains "SELECT students contains Ivan Ivanov" "Ivan Ivanov" "$TMP/out"
assert_contains "SELECT students contains Maria Volkova" "Maria Volkova" "$TMP/out"

run_status "SELECT with WHERE group_id" 0 \
    ./database/vpddb --database "$DB" \
    --query 'SELECT students WHERE group_id = 11'
assert_contains "WHERE group_id = 11 finds student 301" "301" "$TMP/out"
assert_not_contains "WHERE group_id = 11 excludes student 303" "303" "$TMP/out"

run_status "SELECT with ORDER BY name DESC" 0 \
    ./database/vpddb --database "$DB" \
    --query 'SELECT students WHERE group_id = 11 ORDER BY name DESC'
assert_equals "ORDER BY DESC puts Petr before Ivan" "302" \
    "$(awk 'NR == 2 { print $1 }' "$TMP/out")"

run_status "SELECT with two AND conditions" 0 \
    ./database/vpddb --database "$DB" \
    --query 'SELECT students WHERE group_id = 11 AND name = "Ivan Ivanov"'
assert_contains "two conditions find student 301" "301" "$TMP/out"
assert_not_contains "two conditions exclude student 302" "302" "$TMP/out"

run_status "INSERT student succeeds" 0 \
    ./database/vpddb --database "$DB" \
    --query 'INSERT student 400 "Test Student" 11'
assert_contains "INSERT reports assigned id" "inserted student id=400" "$TMP/out"

run_status "INSERT with automatic id succeeds" 0 \
    ./database/vpddb --database "$DB" \
    --query 'INSERT student 0 "Auto Id" 11'
assert_contains "automatic id is 401" "inserted student id=401" "$TMP/out"
run_status "delete automatic-id student" 0 \
    ./database/vpddb --database "$DB" \
    --query 'DELETE students WHERE id = 401'

run_status "duplicate primary key is rejected" 74 \
    ./database/vpddb --database "$DB" \
    --query 'INSERT student 400 "Duplicate" 11'
assert_contains "duplicate key message mentions id" "already exists" "$TMP/err"

run_status "missing foreign key is rejected" 74 \
    ./database/vpddb --database "$DB" \
    --query 'INSERT student 402 "Bad FK" 999'
assert_contains "foreign key message mentions group" \
    "foreign key violation" "$TMP/err"

run_status "UPDATE student succeeds" 0 \
    ./database/vpddb --database "$DB" \
    --query 'UPDATE students SET group_id = 12 WHERE id = 400'
assert_contains "UPDATE reports one record" "updated 1 student" "$TMP/out"

run_status "updated row is visible" 0 \
    ./database/vpddb --database "$DB" \
    --query 'SELECT students WHERE id = 400'
assert_contains "updated student is in group 12" $'400\tTest Student\t12' "$TMP/out"

run_status "deleting referenced group is rejected" 74 \
    ./database/vpddb --database "$DB" --query 'DELETE groups WHERE id = 11'
assert_contains "referential integrity message mentions student" \
    "references it" "$TMP/err"

run_status "insert new group 99" 0 \
    ./database/vpddb --database "$DB" \
    --query 'INSERT group 99 "TEMP-99" 1'
run_status "insert student 403 in group 99" 0 \
    ./database/vpddb --database "$DB" \
    --query 'INSERT student 403 "Temp Student" 99'
run_status "delete non-empty group 99 is rejected" 74 \
    ./database/vpddb --database "$DB" --query 'DELETE groups WHERE id = 99'
run_status "delete student 403" 0 \
    ./database/vpddb --database "$DB" \
    --query 'DELETE students WHERE id = 403'
run_status "delete now-empty group 99" 0 \
    ./database/vpddb --database "$DB" --query 'DELETE groups WHERE id = 99'

run_status "journal contains mutating operations" 0 test -s "$DB.journal"
assert_contains "journal records INSERT" "INSERT:" "$DB.journal"
assert_contains "journal records UPDATE" "UPDATE:" "$DB.journal"
assert_contains "journal records DELETE" "DELETE:" "$DB.journal"

# ---------------------------------------------------------------------------
# 13–15. JSON export/import and statham.json
# ---------------------------------------------------------------------------

run_status "export database to JSON" 0 \
    ./database/vpddb --database "$DB" --export "$TMP/export.json"
if [ -s "$TMP/export.json" ]; then
    pass "export file is non-empty"
else
    fail "export file is empty or missing"
fi
assert_contains "export contains students array" '"students"' "$TMP/export.json"
assert_contains "export contains student data" '"Ivan Ivanov"' "$TMP/export.json"
assert_contains "export contains faculty foreign keys" '"faculty_id"' "$TMP/export.json"

run_status "import JSON into a new database" 0 \
    ./database/vpddb --database "$TMP/imported.dat" --init \
    --import "$TMP/export.json" --show-stats
assert_contains "import restores six students" "students: 6" "$TMP/out"
run_status "imported database is queryable" 0 \
    ./database/vpddb --database "$TMP/imported.dat" \
    --query 'SELECT students WHERE group_id = 12'
assert_contains "imported student 303 is present" "Anna Sidorova" "$TMP/out"

run_status "json_demo parses statham.json" 0 \
    ./json/json_demo json/statham.json
assert_contains "statham has six quotes" "quotes: 6" "$TMP/out"
assert_contains "statham demonstrates boolean type" "boolean=" "$TMP/out"
assert_contains "statham demonstrates null type" "null=1" "$TMP/out"

printf '%s' '{ "students": [ { "id": 1, "name": "X", } ] }' >"$TMP/bad.json"
printf '%s' '{ "students": [ { "id": 1, "name": "X", "group_id": 999 } ] }' >"$TMP/bad_fk.json"

run_status "damaged JSON syntax is rejected" 65 \
    ./database/vpddb --database "$TMP/imported.dat" --import "$TMP/bad.json"

run_status "JSON with broken foreign key is rejected" 74 \
    ./database/vpddb --database "$TMP/imported.dat" --import "$TMP/bad_fk.json"

# ---------------------------------------------------------------------------
# 16–19. Query DSL: lexer, parser, executor
# ---------------------------------------------------------------------------

run_status "unknown table is a syntax error" 65 \
    ./database/vpddb --database "$DB" --query 'SELECT teachers'
assert_contains "unknown table message" "unknown table" "$TMP/err"

run_status "field of another table is a syntax error" 65 \
    ./database/vpddb --database "$DB" \
    --query 'SELECT students WHERE faculty_id = 1'
assert_contains "foreign field message" "does not belong" "$TMP/err"

run_status "incomplete query is a syntax error" 65 \
    ./database/vpddb --database "$DB" --query 'SELECT students WHERE'
assert_contains "incomplete query message" "parser error" "$TMP/err"

run_status "primary key cannot be updated" 65 \
    ./database/vpddb --database "$DB" \
    --query 'UPDATE students SET id = 9 WHERE id = 301'
assert_contains "primary key update message" "cannot be updated" "$TMP/err"

run_status "DELETE without WHERE is rejected" 65 \
    ./database/vpddb --database "$DB" --query 'DELETE students'
assert_contains "DELETE requires WHERE" "requires a WHERE" "$TMP/err"

printf 'SELECT students WHERE id = 304\n' | \
    ./database/vpddb --database "$DB" --query - >"$TMP/out" 2>"$TMP/err"
assert_contains "query from stdin finds student 304" "Ivan Smirnov" "$TMP/out"

run_status "query file executes several statements" 0 \
    ./database/vpddb --database "$TMP/session.dat" --init --seed
run_status "run demo_session.txt" 0 \
    ./database/vpddb --database "$TMP/session.dat" --file tests/demo_session.txt
assert_contains "query file inserted and found student 306" \
    "Nikolai Orlov" "$TMP/out"
assert_contains "query file deleted student 306" "deleted 1 student" "$TMP/out"

# ---------------------------------------------------------------------------
# 20. Corrupted storage, missing files, config, persistence
# ---------------------------------------------------------------------------

run_status "missing database file is an input error" 66 \
    ./database/vpddb --database "$TMP/does-not-exist.dat" --show-stats

cp "$DB" "$TMP/bad_magic.dat"
printf 'BADDATA!' | dd of="$TMP/bad_magic.dat" bs=1 seek=0 \
    conv=notrunc status=none
run_status "bad magic is detected" 75 \
    ./database/vpddb --database "$TMP/bad_magic.dat" --show-stats

cp "$DB" "$TMP/bad_checksum.dat"
printf 'X' | dd of="$TMP/bad_checksum.dat" bs=1 seek=52 \
    conv=notrunc status=none
run_status "checksum mismatch is detected" 75 \
    ./database/vpddb --database "$TMP/bad_checksum.dat" --show-stats

head -c 20 "$DB" >"$TMP/truncated.dat"
run_status "truncated storage is detected" 75 \
    ./database/vpddb --database "$TMP/truncated.dat" --show-stats

run_status "config.yaml is accepted" 0 \
    ./database/vpddb --config config.yaml --database "$DB" \
    --query 'SELECT faculties'
assert_contains "config run reads database" "Institute of Radio Engineering" "$TMP/out"

printf 'database: %s\nexport_file: %s\nshow_stats: true\n' \
    "$DB" "$TMP/config_export.json" >"$TMP/config.yaml"
run_status "config export_file is used by --export" 0 \
    ./database/vpddb --config "$TMP/config.yaml" --export
if [ -s "$TMP/config_export.json" ]; then
    pass "config export file was created"
else
    fail "config export file was not created"
fi
assert_contains "config show_stats is applied" "students:" "$TMP/out"

run_status "data persists after reopen" 0 \
    ./database/vpddb --database "$DB" --query 'SELECT students WHERE id = 400'
assert_contains "persisted student 400 is present" "Test Student" "$TMP/out"

run_status "help exits with 0" 0 ./database/vpddb --help
assert_contains "help mentions --query" "--query" "$TMP/out"

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

printf '\nTotal: %d passed, %d failed\n' "$PASS" "$FAIL"
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi

exit 0
