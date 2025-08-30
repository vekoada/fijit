CC = clang
CFLAGS = -Wall -Wextra -O2 -Iinclude -fPIC
LDFLAGS =

SRC = src/fijs.c
OBJS = $(patsubst src/%.c, obj/%.o, $(SRC))

STATIC_LIB = lib/libfijs.a
SHARED_LIB = lib/libfijs.so

all: static shared

static: $(STATIC_LIB)
shared: $(SHARED_LIB)

$(STATIC_LIB): $(OBJS) | lib
	ar rcs $@ $^

$(SHARED_LIB): $(OBJS) | lib
	$(CC) -shared $(LDFLAGS) -o $@ $^

obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj lib:
	mkdir -p $@

clean:
	rm -rf obj lib

.PHONY: all static shared clean