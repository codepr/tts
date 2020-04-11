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

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>
#include "tts_client.h"
#include "tts_protocol.h"

static double time_spec_seconds(struct timespec* ts) {
    return (double) ts->tv_sec + (double) ts->tv_nsec * 1.0e-9;
}

static void prompt(tts_client *c) {
    printf("%s:%i> ", c->host, c->port);
}

static void print_tts_response(const struct tts_packet *tts_p) {
    if (tts_p->header.opcode == TTS_ACK) {
        printf(tts_p->header.status == TTS_OK ? "OK\n" : "NOK\n");
    } else if (tts_p->header.opcode == TTS_QUERY_RESPONSE) {
        unsigned long long ts = 0ULL;
        for (size_t i = 0; i < tts_p->query_r.len; ++i) {
            ts = tts_p->query_r.results[i].ts_sec * 1e9 + \
                 tts_p->query_r.results[i].ts_nsec;
            printf("%llu %.4Lf ", ts, tts_p->query_r.results[i].value);
            for (size_t j = 0; j < tts_p->query_r.results[i].labels_len; ++j) {
                printf("%s %s ",
                       tts_p->query_r.results[i].labels[j].label,
                       tts_p->query_r.results[i].labels[j].value);
            }
            printf("\n");
        }
    }
}

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;
    size_t line_len = 0LL;
    char *line = NULL;
    struct tts_packet tts_p;
    double delta = 0.0;
    tts_client c;
    tts_client_init(&c, "127.0.0.1", 19191);
    tts_client_connect(&c);
    struct timespec tstart = {0,0}, tend = {0,0};
    while (1) {
        prompt(&c);
        getline(&line, &line_len, stdin);
        (void) clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tstart);
        if (tts_client_send_command(&c, line) == TTS_CLIENT_FAILURE) {
            printf("Unknown command or malformed one\n");
            continue;
        }
        tts_client_recv_response(&c, &tts_p);
        (void) clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tend);
        print_tts_response(&tts_p);
        if (tts_p.header.opcode == TTS_QUERY_RESPONSE) {
            delta = time_spec_seconds(&tend) - time_spec_seconds(&tstart);
            printf("%lu results in %lf seconds.\n", tts_p.query_r.len, delta);
        }
    }
    tts_client_destroy(&c);
    free(line);
    return 0;
}
