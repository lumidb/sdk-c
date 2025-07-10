CC = clang

LIBS = $(shell pkg-config --cflags --libs libcjson libcurl)
CFLAGS = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wno-unused-parameter
# CFLAGS += -g -fsanitize=address

TARGET = build/lumilapio
SOURCES = lumilapio.c

$(TARGET): $(SOURCES) lumidb.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(LIBS) -o $@ $(SOURCES)

clean:
	rm -rvf build

.PHONY: clean
