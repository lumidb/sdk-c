CC = clang

LIBS = $(shell pkg-config --cflags --libs libcjson libcurl)
CFLAGS = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wno-unused-parameter
# CFLAGS += -g -fsanitize=address

TARGET = dist/lumilapio
SOURCES = upload.c

$(TARGET): $(SOURCES) lumidb.h
	@mkdir -p dist
	$(CC) $(CFLAGS) $(LIBS) -o $@ $(SOURCES)

clean:
	rm -rvf dist

.PHONY: clean
