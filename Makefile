CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -Iinclude
LDFLAGS = -lm

SRCS    = src/conservation_guardian.c
TESTSRC = tests/test_conservation_guardian.c

.PHONY: all clean test

all: build/test_runner

build/test_runner: $(SRCS) $(TESTSRC) | build
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(TESTSRC) $(LDFLAGS)

build:
	mkdir -p build

test: build/test_runner
	@./build/test_runner

clean:
	rm -rf build
