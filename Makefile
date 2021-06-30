CC = gcc

CFLAGS = -g -fsanitize=address -fsanitize=undefined -fsanitize=bounds
LDFLAGS =

.PHONY: all main clean

all: main

main:
	$(CC) main.c $(CFLAGS) -o main

clean:
	@rm main
