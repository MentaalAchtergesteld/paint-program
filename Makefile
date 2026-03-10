CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O2 -std=c99
INCLUDES = -I.

LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

SRC = main.c gui.h
OUT = paint

all: $(OUT)

$(OUT): $(SRC)
				$(CC) $(CFLAGS) $(INCLUDES) $(SRC) -o $(OUT) $(LDFLAGS)

run: $(OUT)
			./$(OUT)

clean: rm -f $(OUT)

.PHONY: all run clean
