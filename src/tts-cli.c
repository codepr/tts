/* BSD 2-Clause License
 *
 * Copyright (c) 2020, Andrea Giacomo Baldan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE     2048
#define COMMANDS_NR 4

typedef void (*tts_cmd_handler)(char *);

static void tts_handle_create(char *);
static void tts_handle_delete(char *);
static void tts_handle_addpoints(char *);
static void tts_handle_query(char *);
//static void tts_handle_ack(char *);
//static void tts_handle_query_response(char *);

static const char *cmds[COMMANDS_NR] = {
    "tts_create",
    "tts_delete",
    "tts_addpoints",
    "tts_query"
};

static tts_cmd_handler handlers[4] = {
    tts_handle_create,
    tts_handle_delete,
    tts_handle_addpoints,
    tts_handle_query
};

static void tts_handle_create(char *line) {
    (void) line;
}

static void tts_handle_delete(char *line) {
    (void) line;
}

static void tts_handle_addpoints(char *line) {
    (void) line;

}

static void tts_handle_query(char *line) {
    (void) line;

}

//static void tts_handle_ack(char *line) {
//    (void) line;
//
//}
//
//static void tts_handle_query_response(char *line) {
//    (void) line;
//
//}

static void parse_cmd(char *cmd) {
    int cmd_id = -1, i;
    for (i = 0; i < COMMANDS_NR && cmd_id == -1; ++i) {
        if (strncasecmp(cmd, cmds[i], strlen(cmds[i])) == 0)
            cmd_id = i;
    }
    if (cmd_id == -1)
        printf("Unknown command\n");
    else
        handlers[i](cmd + strlen(cmds[i]));
}

static void prompt(void) {
    printf("> ");
}

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;
    size_t line_len = 0LL;
    char *line = NULL;
    while (1) {
        prompt();
        getline(&line, &line_len, stdin);
        parse_cmd(line);
    }
    free(line);
    return 0;
}
