CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g
SRC = src
SOURCES = $(SRC)/errors.c $(SRC)/utils.c $(SRC)/hash.c $(SRC)/sha256.c \
          $(SRC)/backend.c $(SRC)/blob.c $(SRC)/cas.c $(SRC)/snapshot.c \
          $(SRC)/branch.c $(SRC)/vfs.c $(SRC)/namespace.c $(SRC)/minnas.c $(SRC)/cli.c
all: build/minnas
build/minnas: $(SOURCES)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SOURCES) -lm
clean:
	rm -rf build
.PHONY: all clean
