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
#include <unistd.h>
#include "tts_client.h"
#include "tts_protocol.h"

#define LOCALHOST    "127.0.0.1"
#define DEFAULT_PORT 19191

static const char *flag_description[] = {
    "Print this help",
    "Set the execution mode, the connection to use, accepts inet|unix",
    "Set an address hostname to listen on",
    "Set a different port other than 19191",
};

static void print_help(const char *me) {
    printf("\ntts - Transient Time Series CLI\n\n");
    printf("Usage: %s [-a addr] [-p port] [-m mode] [-h]\n\n", me);
    const char flags[4] = "hmap";
    for (int i = 0; i < 4; ++i)
        printf(" -%c: %s\n", flags[i], flag_description[i]);
    printf("\n");
}

static const char *cmd_usage(const char *cmd) {
    if (strncasecmp(cmd, "create", 6) == 0)
        return "CREATE timeseries-name [retention]";
    if (strncasecmp(cmd, "delete", 6) == 0)
        return "DELETE timeseries-name";
    if (strncasecmp(cmd, "add", 3) == 0)
        return "ADD timeseries-name timestamp|* value [label value ..] - ..";
    if (strncasecmp(cmd, "query", 5) == 0)
        return "QUERY timeseries-name [>|<|RANGE] start_timestamp [end_timestamp] [AVG value]";
    return NULL;
}

static int modetoi(const char *str) {
    if (strcasecmp(str, "inet") == 0)
        return AF_INET;
    if (strcasecmp(str, "unix") == 0)
        return AF_UNIX;
    return -1;
}

static double time_spec_seconds(struct timespec* ts) {
    return (double) ts->tv_sec + (double) ts->tv_nsec * 1.0e-9;
}

static void prompt(tts_client *c) {
    if (c->opts->s_family == AF_INET)
        printf("%s:%i> ", c->opts->s_addr, c->opts->s_port);
    else if (c->opts->s_family == AF_UNIX)
        printf("%s> ", c->opts->s_addr);
}

static const char *errors_description[] = {
    "OK",
    "NOK - Timeseries doesn't exist",
    "NOK - Timeseries already exists",
    "NOK - Server rejected command: unknown command",
    "NOK - Server rejected command: Out of memory"
};

static void print_tts_response(const struct tts_packet *tts_p) {
    if (tts_p->header.opcode == TTS_ACK) {
        printf("%s\n", errors_description[tts_p->header.status]);
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
    int opt, port = DEFAULT_PORT, mode = AF_INET;
    char *host = LOCALHOST;
    size_t line_len = 0LL;
    char *line = NULL;
    struct tts_packet tts_p;
    double delta = 0.0;
    while ((opt = getopt(argc, argv, "h:m:a:p:h:")) != -1) {
        switch (opt) {
            case 'm':
                mode = modetoi(optarg);
                if (mode == -1) {
                    fprintf(stderr, "Unknown mode '%s'\n", optarg);
                    print_help(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'a':
                host = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                print_help(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    tts_client c;
    struct tts_connect_options conn_opts;
    memset(&conn_opts, 0x00, sizeof(conn_opts));
    conn_opts.s_family = mode;
    conn_opts.s_addr = host;
    conn_opts.s_port = port;
    tts_client_init(&c, &conn_opts);
    if (tts_client_connect(&c) < 0)
        exit(EXIT_FAILURE);
    int err = 0;
    struct timespec tstart = {0,0}, tend = {0,0};
    while (1) {
        prompt(&c);
        getline(&line, &line_len, stdin);
        (void) clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tstart);
        err = tts_client_send_command(&c, line);
        if (err <= 0) {
            if (err == TTS_CLIENT_SUCCESS) {
                tts_client_disconnect(&c);
                break;
            } else if (err == TTS_CLIENT_UNKNOWN_CMD) {
                printf("Unknown command or malformed one\n");
                const char *usage = cmd_usage(line);
                if (usage)
                    printf("\nSuggesed usage: %s\n\n", usage);
            } else if (err == TTS_CLIENT_FAILURE) {
                printf("Couldn't send the command: %s\n", strerror(errno));
            }
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
