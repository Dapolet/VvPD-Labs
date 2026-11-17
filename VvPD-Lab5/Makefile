# VvPD-Lab5 — CLI, streams, file database, CRUD, JSON and a small query DSL
#
# On macOS gcc is usually Apple Clang. Prefer Homebrew GCC when available.

CC := $(shell command -v gcc-16 2>/dev/null || echo cc)
CLANG ?= clang

CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -O2 -g
CPPFLAGS ?= -Idatabase -Iquery -Ijson -Ifiletool
LDLIBS ?= -lm

SANITIZE_FLAGS := -std=c17 -Wall -Wextra -Wpedantic -O1 -g \
                  -fsanitize=address,undefined -fno-omit-frame-pointer

ARTIFACTS := artifacts

DB_COMMON := database/model.c database/storage.c database/database.c \
             database/config.c
QUERY_SRC := query/lexer.c query/parser.c query/executor.c
JSON_SRC := json/json.c json/database_json.c

BINS := filetool/filetool filetool/argv_demo filetool/argv_void_demo \
        database/vpddb json/json_demo

.PHONY: all filetool database json test artifacts sanitize clean distclean help

all: $(BINS)

filetool: filetool/filetool filetool/argv_demo filetool/argv_void_demo
database: database/vpddb
json: json/json_demo

filetool/filetool: filetool/filetool.c
	$(CC) $(CFLAGS) $< -o $@

filetool/argv_demo: filetool/argv_demo.c
	$(CC) $(CFLAGS) $< -o $@

filetool/argv_void_demo: filetool/argv_demo.c
	$(CC) $(CFLAGS) -DNO_ARGS $< -o $@

database/vpddb: database/main.c $(DB_COMMON) $(QUERY_SRC) $(JSON_SRC) \
                database/model.h database/storage.h database/database.h \
                database/config.h query/query.h json/json.h json/database_json.h
	$(CC) $(CFLAGS) $(CPPFLAGS) database/main.c $(DB_COMMON) \
	    $(QUERY_SRC) $(JSON_SRC) -o $@ $(LDLIBS)

json/json_demo: json/json_demo.c json/json.c json/json.h
	$(CC) $(CFLAGS) $(CPPFLAGS) json/json_demo.c json/json.c -o $@ $(LDLIBS)

test: all
	@mkdir -p $(ARTIFACTS)
	@bash tests/run_tests.sh > $(ARTIFACTS)/tests.txt 2>&1 || \
	    { cat $(ARTIFACTS)/tests.txt; exit 1; }
	@cat $(ARTIFACTS)/tests.txt

artifacts: all
	@bash tests/make_artifacts.sh $(ARTIFACTS)

sanitize: all
	@mkdir -p $(ARTIFACTS)
	$(CLANG) $(SANITIZE_FLAGS) $(CPPFLAGS) database/main.c $(DB_COMMON) \
	    $(QUERY_SRC) $(JSON_SRC) -o $(ARTIFACTS)/vpddb_asan $(LDLIBS)
	$(CLANG) $(SANITIZE_FLAGS) $(CPPFLAGS) json/json_demo.c json/json.c \
	    -o $(ARTIFACTS)/json_demo_asan $(LDLIBS)
	$(CLANG) $(SANITIZE_FLAGS) $(CPPFLAGS) filetool/filetool.c \
	    -o $(ARTIFACTS)/filetool_asan
	@rm -f $(ARTIFACTS)/asan_students.dat $(ARTIFACTS)/asan_students.dat.*
	@$(ARTIFACTS)/vpddb_asan \
	    --database $(ARTIFACTS)/asan_students.dat --init --seed \
	    > $(ARTIFACTS)/asan_database.txt 2>&1
	@$(ARTIFACTS)/vpddb_asan \
	    --database $(ARTIFACTS)/asan_students.dat \
	    --query 'SELECT students WHERE group_id = 11 ORDER BY name' \
	    >> $(ARTIFACTS)/asan_database.txt 2>&1
	@$(ARTIFACTS)/vpddb_asan \
	    --database $(ARTIFACTS)/asan_students.dat \
	    --query 'INSERT student 900 "ASAN Test" 11' \
	    >> $(ARTIFACTS)/asan_database.txt 2>&1
	@$(ARTIFACTS)/vpddb_asan \
	    --database $(ARTIFACTS)/asan_students.dat \
	    --export $(ARTIFACTS)/asan_export.json \
	    >> $(ARTIFACTS)/asan_database.txt 2>&1
	@$(ARTIFACTS)/json_demo_asan json/statham.json \
	    > $(ARTIFACTS)/asan_json.txt 2>&1
	@$(ARTIFACTS)/filetool_asan \
	    --input-file tests/sample_input.txt \
	    --output-file $(ARTIFACTS)/asan_copy.txt --show-stats \
	    > $(ARTIFACTS)/asan_filetool.txt 2>&1
	@echo "sanitizer runs completed; see artifacts/asan_*.txt"

clean:
	rm -rf $(BINS) $(ARTIFACTS)/vpddb_asan $(ARTIFACTS)/json_demo_asan \
	       $(ARTIFACTS)/filetool_asan
	rm -rf $(wildcard *.dSYM) $(wildcard */*.dSYM) \
	       $(wildcard $(ARTIFACTS)/*.dSYM)
	rm -f students.dat students.dat.tmp students.dat.journal \
	      export.json import.json
	rm -f $(ARTIFACTS)/asan_students.dat $(ARTIFACTS)/asan_students.dat.* \
	      $(ARTIFACTS)/asan_export.json $(ARTIFACTS)/asan_copy.txt

distclean: clean
	rm -rf $(ARTIFACTS)
	rm -f tests/*.tmp

help:
	@echo "make            — build filetool, vpddb and json_demo"
	@echo "make test       — run the regression tests"
	@echo "make artifacts  — generate artifact logs in artifacts/"
	@echo "make sanitize   — build and run ASan/UBSan checks"
	@echo "make clean      — remove binaries and generated databases"
