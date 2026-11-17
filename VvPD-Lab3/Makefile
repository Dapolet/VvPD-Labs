CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -O2
PYTHON ?= python3

STRINGS := strings
REGEX := regex
BINARIES := $(STRINGS)/ascii $(STRINGS)/utf8 $(STRINGS)/types \
            $(STRINGS)/unicode_identifiers $(REGEX)/generator

.PHONY: all strings regex generator web test clean

all: strings generator

strings: $(STRINGS)/ascii $(STRINGS)/utf8 $(STRINGS)/types $(STRINGS)/unicode_identifiers

$(STRINGS)/%: $(STRINGS)/%.c
	$(CC) $(CFLAGS) $< -o $@

generator: $(REGEX)/generator

$(REGEX)/generator: $(REGEX)/generator.c
	$(CC) $(CFLAGS) $< -o $@

regex: generator
	cd $(REGEX) && ./generator > VPD_Lab3.txt
	$(PYTHON) $(REGEX)/regex.py

web:
	@echo "Откройте web/index.html в браузере или запустите:"
	@echo "  $(PYTHON) -m http.server 8000"
	@echo "  http://localhost:8000/web/"

test: all
	$(STRINGS)/ascii
	$(STRINGS)/utf8
	$(STRINGS)/types
	$(STRINGS)/unicode_identifiers
	$(PYTHON) $(STRINGS)/encoding.py
	$(PYTHON) $(STRINGS)/base64_demo.py
	@if command -v node >/dev/null 2>&1; then \
		node --check $(REGEX)/../web/validation.js; \
		node $(REGEX)/../web/selftest.js; \
	else \
		echo "node not found: web/selftest.js skipped"; \
	fi

clean:
	rm -f $(BINARIES)
	rm -rf $(STRINGS)/__pycache__ $(REGEX)/__pycache__
