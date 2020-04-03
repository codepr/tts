.POSIX:
CC=gcc
INCLUDE_DIR=include
CFLAGS=-std=c11 -Wall -Wextra -Werror -pedantic -D_DEFAULT_SOURCE=200809L -I$(INCLUDE_DIR) -ggdb -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -pg

.PHONY:
	tts clean

tts: src/*.c include/*.h
	$(CC) $(CFLAGS) src/tts.c src/tts_server.c src/tts_protocol.c src/pack.c src/tts_handlers.c -o tts

tts-cli: src/tts-cli.c
	$(CC) $(CFLAGS) src/tts-cli.c -o tts-cli

clean:
	@rm tts
