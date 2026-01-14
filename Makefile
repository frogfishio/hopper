# SPDX-FileCopyrightText: 2026 Frogfish
# SPDX-License-Identifier: Apache-2.0
# Author: Alexander Croft <alex@frogfish.io>

CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -Iinclude
AR ?= ar
SONAME ?= 1
PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig

SRC := src/hopper.c src/pic.c
OBJ := $(SRC:.c=.o)
EXAMPLES := examples/basic
PY_EXAMPLE := bindings/python/example.py
RUST_DIR := bindings/rust

TEST_BIN := test/bin/hopper_tests
SHARED := libhopper.so
STATIC := libhopper.a
PCFILE := hopper.pc

.PHONY: all clean test check python-example rust-example catalog-load
all: $(STATIC) $(SHARED)

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(STATIC): $(OBJ)
	$(AR) rcs $@ $^

$(SHARED): $(OBJ)
	$(CC) -shared -Wl,-soname,libhopper.so.$(SONAME) -o $@ $^

test: $(STATIC) $(SHARED) $(TEST_BIN)
	$(TEST_BIN)

check: test

$(TEST_BIN): test/main.c $(STATIC) | test/bin
	$(CC) $(CFLAGS) -o $@ test/main.c $(STATIC)

examples: $(EXAMPLES)

examples/basic: examples/basic.c $(STATIC)
	$(CC) $(CFLAGS) -o $@ examples/basic.c $(STATIC)

python-example: $(STATIC)
	HOPPER_LIB=./libhopper.so python3 $(PY_EXAMPLE)

rust-example:
	cd $(RUST_DIR) && cargo build

catalog-load: $(STATIC)
	HOPPER_LIB=./libhopper.so python3 tools/load_catalog.py tools/catalog_example.json

test/bin:
	mkdir -p test/bin

clean:
	rm -f $(OBJ) $(STATIC) $(SHARED) $(TEST_BIN) $(PCFILE)

install: $(STATIC) $(SHARED) $(PCFILE)
	mkdir -p $(DESTDIR)$(INCLUDEDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(PKGCONFIGDIR)
	cp include/hopper.h $(DESTDIR)$(INCLUDEDIR)/
	cp $(STATIC) $(DESTDIR)$(LIBDIR)/
	cp $(SHARED) $(DESTDIR)$(LIBDIR)/
	cp $(PCFILE) $(DESTDIR)$(PKGCONFIGDIR)/

$(PCFILE):
	@echo "prefix=$(PREFIX)" > $(PCFILE)
	@echo "exec_prefix=\$${prefix}" >> $(PCFILE)
	@echo "libdir=\$${exec_prefix}/lib" >> $(PCFILE)
	@echo "includedir=\$${prefix}/include" >> $(PCFILE)
	@echo "" >> $(PCFILE)
	@echo "Name: hopper" >> $(PCFILE)
	@echo "Description: Hopper runtime" >> $(PCFILE)
	@echo "Version: 1.0.0" >> $(PCFILE)
	@echo "Cflags: -I\$${includedir}" >> $(PCFILE)
	@echo "Libs: -L\$${libdir} -lhopper" >> $(PCFILE)
