.POSIX:
INCLUDE_DIR=./include
CFLAGS=-std=c99 -Wall -Wextra -Werror -pedantic -D_DEFAULT_SOURCE=200809L -I$(INCLUDE_DIR) -ggdb -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -pg

.PHONY:
	tts clean

tts: src/tts.c include/tts_vector.h
	$(CC) $(CFLAGS) src/tts.c -o tts

clean:
	@rm tts
