# VvPD-Lab4 — pointers, dynamic memory, structures, files, fixed point, BigUInt
#
# На macOS команда gcc обычно является Apple Clang. Если установлен Homebrew
# GCC, используем его; иначе — cc/c++.

CC := $(shell command -v gcc-16 2>/dev/null || echo cc)
CXX := $(shell command -v g++-16 2>/dev/null || echo c++)
CLANG ?= clang

CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -O2
CXXFLAGS ?= -std=gnu++17 -Wall -Wextra -Wpedantic
SANITIZE_FLAGS := -std=c17 -Wall -Wextra -Wpedantic -O1 -g \
                  -fsanitize=address,undefined -fno-omit-frame-pointer
LDLIBS ?= -lm

ARTIFACTS := artifacts

POINTER_BINS := pointers/pointers pointers/arrays pointers/dynamic
STRUCT_BINS := structs_files/structs structs_files/text_file \
               structs_files/binary_file structs_files/pointer_trap
CPP_BINS := cpp_memory/new_delete cpp_memory/raii
FIXED_BINS := fixed/fixed_demo
LINKED_BINS := linked_number/linked_number
BIGINT_BINS := bigint/bigint_demo
BENCH_BINS := bigint/benchmark_O0 bigint/benchmark_O2

ALL_BINS := $(POINTER_BINS) $(STRUCT_BINS) $(CPP_BINS) $(FIXED_BINS) \
            $(LINKED_BINS) $(BIGINT_BINS)

.PHONY: all pointers structs_files cpp_memory fixed linked_number bigint \
        bench bench-run test artifacts sanitize clean distclean help

all: $(ALL_BINS)

pointers: $(POINTER_BINS)
structs_files: $(STRUCT_BINS)
cpp_memory: $(CPP_BINS)
fixed: $(FIXED_BINS)
linked_number: $(LINKED_BINS)
bigint: $(BIGINT_BINS)
bench: $(BENCH_BINS)

$(POINTER_BINS): %: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

$(STRUCT_BINS): %: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

$(CPP_BINS): %: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

fixed/fixed_demo: fixed/main.c fixed/fixed.c fixed/fixed.h
	$(CC) $(CFLAGS) -Ifixed fixed/main.c fixed/fixed.c -o $@ $(LDLIBS)

linked_number/linked_number: linked_number/main.c linked_number/digit.c linked_number/digit.h
	$(CC) $(CFLAGS) -Ilinked_number linked_number/main.c linked_number/digit.c -o $@ $(LDLIBS)

bigint/bigint_demo: bigint/main.c bigint/bigint.c bigint/bigint.h
	$(CC) $(CFLAGS) -Ibigint bigint/main.c bigint/bigint.c -o $@ $(LDLIBS)

bigint/benchmark_O0: bigint/benchmark.cpp
	$(CXX) $(CXXFLAGS) -O0 -DBENCH_OPT=0 $< -o $@

bigint/benchmark_O2: bigint/benchmark.cpp
	$(CXX) $(CXXFLAGS) -O2 -DBENCH_OPT=2 $< -o $@

test: all
	@mkdir -p $(ARTIFACTS)
	./pointers/pointers
	./pointers/arrays
	./pointers/dynamic
	./structs_files/structs
	./structs_files/text_file $(ARTIFACTS)/numbers.txt
	./structs_files/binary_file $(ARTIFACTS)/students.dat
	./structs_files/pointer_trap $(ARTIFACTS)/pointer_trap.dat
	./cpp_memory/new_delete
	./cpp_memory/raii
	./fixed/fixed_demo
	./linked_number/linked_number
	./bigint/bigint_demo

artifacts: all
	@mkdir -p $(ARTIFACTS)
	./pointers/pointers > $(ARTIFACTS)/run_pointers.txt
	./pointers/arrays > $(ARTIFACTS)/run_arrays.txt
	./pointers/dynamic > $(ARTIFACTS)/run_dynamic.txt
	./structs_files/structs > $(ARTIFACTS)/run_structs.txt
	./structs_files/text_file $(ARTIFACTS)/numbers.txt > $(ARTIFACTS)/run_text_file.txt
	./structs_files/binary_file $(ARTIFACTS)/students.dat > $(ARTIFACTS)/run_binary_file.txt
	./structs_files/pointer_trap $(ARTIFACTS)/pointer_trap.dat > $(ARTIFACTS)/run_pointer_trap.txt
	./cpp_memory/new_delete > $(ARTIFACTS)/run_new_delete.txt
	./cpp_memory/raii > $(ARTIFACTS)/run_raii.txt
	./fixed/fixed_demo > $(ARTIFACTS)/run_fixed.txt
	./linked_number/linked_number > $(ARTIFACTS)/run_linked_number.txt
	./bigint/bigint_demo > $(ARTIFACTS)/run_bigint.txt

bench-run: bench
	@mkdir -p $(ARTIFACTS)
	./bigint/benchmark_O0 > $(ARTIFACTS)/benchmark_O0.txt
	./bigint/benchmark_O2 > $(ARTIFACTS)/benchmark_O2.txt
	cat $(ARTIFACTS)/benchmark_O0.txt $(ARTIFACTS)/benchmark_O2.txt > $(ARTIFACTS)/benchmark.txt
	@cat $(ARTIFACTS)/benchmark.txt

sanitize:
	@mkdir -p $(ARTIFACTS)
	$(CLANG) $(SANITIZE_FLAGS) pointers/dynamic.c -o /tmp/vpd_dynamic_asan
	/tmp/vpd_dynamic_asan > $(ARTIFACTS)/asan_dynamic.txt 2>&1
	$(CLANG) $(SANITIZE_FLAGS) -Ifixed fixed/main.c fixed/fixed.c -o /tmp/vpd_fixed_asan
	/tmp/vpd_fixed_asan > $(ARTIFACTS)/asan_fixed.txt 2>&1
	$(CLANG) $(SANITIZE_FLAGS) -Ilinked_number linked_number/main.c linked_number/digit.c -o /tmp/vpd_linked_asan
	/tmp/vpd_linked_asan > $(ARTIFACTS)/asan_linked_number.txt 2>&1
	$(CLANG) $(SANITIZE_FLAGS) -Ibigint bigint/main.c bigint/bigint.c -o /tmp/vpd_bigint_asan
	/tmp/vpd_bigint_asan > $(ARTIFACTS)/asan_bigint.txt 2>&1
	@echo "sanitizer runs completed; see artifacts/asan_*.txt"

clean:
	rm -f $(ALL_BINS) $(BENCH_BINS)
	rm -f numbers.txt students.dat pointer_trap.dat raii_demo.txt
	rm -f $(ARTIFACTS)/numbers.txt $(ARTIFACTS)/students.dat $(ARTIFACTS)/pointer_trap.dat

distclean: clean
	rm -rf $(ARTIFACTS)

help:
	@echo "make            — собрать все демонстрации"
	@echo "make test       — собрать и запустить все демонстрации"
	@echo "make artifacts  — сохранить выводы в artifacts/"
	@echo "make bench-run  — собрать и запустить бенчмарк -O0/-O2"
	@echo "make sanitize   — прогнать ключевые программы под ASan/UBSan"
	@echo "make clean      — удалить исполняемые файлы и временные данные"
