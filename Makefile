.POSIX:
CC=gcc
INCLUDE_DIR=include
CFLAGS=-std=c11 -Wall -Wextra -Werror -pedantic -D_DEFAULT_SOURCE=200809L -I$(INCLUDE_DIR) -DHAVE_OPENSSL=1 -lssl -lcrypto -ggdb -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -pg

.PHONY:
	tts clean

tts: src/*.c include/*.h
	$(CC) $(CFLAGS) src/tts_log.c src/tts.c src/tts_server.c src/tts_protocol.c src/pack.c src/tts_handlers.c src/tts_config.c -o tts

tts-cli: src/tts_protocol.c src/tts_protocol.h src/tts_client.h src/tts_client.c src/tts-cli.c
	$(CC) $(CFLAGS) src/tts-cli.c src/tts_protocol.c src/tts_client.c src/pack.c -o tts-cli

clean:
	@rm tts
