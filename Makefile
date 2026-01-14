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
OS := $(shell uname -s)

SHARED_EXT := so
SHARED_LDFLAGS := -shared -Wl,-soname,libhopper.so.$(SONAME)
SONAME_LINK := libhopper.so.$(SONAME)

ifeq ($(OS),Darwin)
SHARED_EXT := dylib
SHARED_LDFLAGS := -dynamiclib -Wl,-install_name,@rpath/libhopper.$(SHARED_EXT)
SONAME_LINK :=
endif

SRC := src/hopper.c src/pic.c
OBJ := $(SRC:.c=.o)
EXAMPLES := examples/basic
PY_EXAMPLE := bindings/python/example.py
RUST_DIR := bindings/rust

TEST_BIN := test/bin/hopper_tests
SHARED := libhopper.$(SHARED_EXT)
STATIC := libhopper.a
PCFILE := hopper.pc
DISTDIR := dist

.PHONY: all clean test check python-example rust-example catalog-load
all: $(STATIC) $(SHARED)

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(STATIC): $(OBJ)
	$(AR) rcs $@ $^

$(SHARED): $(OBJ)
	$(CC) $(SHARED_LDFLAGS) -o $@ $^

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
	rm -rf $(OBJ) $(STATIC) $(SHARED) $(TEST_BIN) $(PCFILE) $(DISTDIR)

install: $(STATIC) $(SHARED) $(PCFILE)
	mkdir -p $(DESTDIR)$(INCLUDEDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(PKGCONFIGDIR)
	cp include/hopper.h $(DESTDIR)$(INCLUDEDIR)/
	cp $(STATIC) $(DESTDIR)$(LIBDIR)/
	cp $(SHARED) $(DESTDIR)$(LIBDIR)/
	cp $(PCFILE) $(DESTDIR)$(PKGCONFIGDIR)/

dist: $(STATIC) $(SHARED) $(PCFILE)
	rm -rf $(DISTDIR)
	mkdir -p $(DISTDIR)/lib $(DISTDIR)/include $(DISTDIR)/pkgconfig
	cp include/hopper.h $(DISTDIR)/include/
	cp $(STATIC) $(DISTDIR)/lib/
	cp $(SHARED) $(DISTDIR)/lib/
ifneq ($(SONAME_LINK),)
	ln -sf $(notdir $(SHARED)) $(DISTDIR)/lib/$(SONAME_LINK)
endif
	cp $(PCFILE) $(DISTDIR)/pkgconfig/

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
