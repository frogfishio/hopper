CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -Iinclude
AR ?= ar

SRC := src/hopper.c src/pic.c
OBJ := $(SRC:.c=.o)

TEST_BIN := test/bin/hopper_tests

.PHONY: all clean test

all: libhopper.a

libhopper.a: $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: libhopper.a $(TEST_BIN)
	$(TEST_BIN)

$(TEST_BIN): test/main.c libhopper.a | test/bin
	$(CC) $(CFLAGS) -o $@ test/main.c libhopper.a

test/bin:
	mkdir -p test/bin

clean:
	rm -f $(OBJ) libhopper.a $(TEST_BIN)
