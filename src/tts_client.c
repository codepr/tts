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
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include "pack.h"
#include "tts_protocol.h"
#include "tts_client.h"

#define BUFSIZE             2048
#define COMMANDS_NR         4

typedef int (*tts_cmd_handler)(char *, struct tts_packet *);

static int tts_handle_new(char *, struct tts_packet *);
static int tts_handle_del(char *, struct tts_packet *);
static int tts_handle_add(char *, struct tts_packet *);
static int tts_handle_query(char *, struct tts_packet *);

static const char *cmds[COMMANDS_NR] = {
    "new",
    "del",
    "add",
    "query"
};

static tts_cmd_handler handlers[4] = {
    tts_handle_new,
    tts_handle_del,
    tts_handle_add,
    tts_handle_query
};

static inline unsigned count_tokens(const char *str, char delim) {
    unsigned count = 0;
    char *ptr = (char *) str;
    while ((ptr = strchr(ptr, delim))) {
           count++;
           ptr++;
    }
    return count;
}

static inline void remove_newline(char *str) {
    char *ptr = strstr(str, "\n");
    *ptr = '\0';
}

static inline unsigned get_digits(unsigned long long n) {
    unsigned count = 0;
    do {
        count++;
        n /= 10;
    } while (n != 0);
    return count;
}

static inline long long read_number(const char *str) {
    char *nul = NULL;
    long long n = strtoll(str, &nul, 10);
    if (*nul != '\0' || nul == str ||
        (get_digits(n) < 10 && get_digits(n) > 13))
        return TTS_CLIENT_FAILURE;
    return n;
}

static inline long double read_real(const char *str) {
    char *nul = NULL;
    long double n = strtold(str, &nul);
    if (*nul != '\0' || nul == str)
        return TTS_CLIENT_FAILURE;
    return n;
}

static int tts_handle_new(char *line, struct tts_packet *tts_p) {
    if (count_tokens(line, ' ') < 1)
        return TTS_CLIENT_FAILURE;
    TTS_SET_REQUEST_HEADER(tts_p, TTS_CREATE_TS);
    struct tts_create_ts *create = &tts_p->create;
    char *token = strtok(line, " ");
    if (!token)
        return TTS_CLIENT_FAILURE;
    create->ts_name_len = strlen(token);
    create->ts_name = malloc(create->ts_name_len + 1);
    snprintf((char *) create->ts_name, create->ts_name_len + 1, "%s", token);
    token = strtok(NULL, " ");
    create->retention = token ? atoi(token) : 0;
    return TTS_CLIENT_SUCCESS;
}

static int tts_handle_del(char *line, struct tts_packet *tts_p) {
    if (count_tokens(line, ' ') < 1)
        return TTS_CLIENT_FAILURE;
    TTS_SET_REQUEST_HEADER(tts_p, TTS_DELETE_TS);
    struct tts_delete_ts *drop = &tts_p->drop;
    char *token = strtok(line, " ");
    if (!token)
        return TTS_CLIENT_FAILURE;
    drop->ts_name_len = strlen(token);
    drop->ts_name = malloc(drop->ts_name_len + 1);
    snprintf((char *) drop->ts_name, drop->ts_name_len + 1, "%s", token);
    return TTS_CLIENT_SUCCESS;
}

static int tts_handle_add(char *line, struct tts_packet *tts_p) {
    if (count_tokens(line, ' ') < 3)
        return TTS_CLIENT_FAILURE;
    TTS_SET_REQUEST_HEADER(tts_p, TTS_ADDPOINTS);
    struct tts_addpoints *points = &tts_p->addpoints;
    char *end_str, *end_val, *vals = NULL;
    char *token = strtok_r(line, " ", &end_str);
    if (!token)
        return TTS_CLIENT_FAILURE;
    points->ts_name_len = strlen(token);
    points->ts_name = malloc(points->ts_name_len + 1);
    snprintf((char *) points->ts_name, points->ts_name_len + 1, "%s", token);
    int j = 0, psize = 4, vsize = 4;
    points->points = calloc(psize, sizeof(*points->points));
    for (int i = 0; (token = strtok_r(NULL, "-", &end_str)); ++i) {
        if (i == psize - 1) {
            psize *= 2;
            points->points =
                realloc(points->points, psize * sizeof(*points->points));
        }
        vsize = 4;
        points->points[i].labels =
            calloc(vsize, sizeof(*points->points[i].labels));
        j = 0;
        vals = strtok_r(token, " ", &end_val);
        if (!vals)
            return TTS_CLIENT_FAILURE;
        if (strcmp(vals, "*") == 0) {
            points->points[i].bits.ts_sec_set = 0;
            points->points[i].bits.ts_nsec_set = 0;
        } else {
            unsigned long long n = read_number(vals);
            int len = get_digits(n);
            points->points[i].bits.ts_sec_set = 1;
            points->points[i].bits.ts_nsec_set = 1;
            if (len == 10)
                points->points[i].ts_sec = n;
            else if (len == 13)
                points->points[i].ts_sec = n / 1e3;
            points->points[i].ts_nsec = 0;
        }
        vals = strtok_r(NULL, " ", &end_val);
        points->points[i].value = read_real(vals);
        while ((vals = strtok_r(NULL, " ", &end_val))) {
            if (j == vsize - 1) {
                vsize *= 2;
                points->points[i].labels =
                    realloc(points->points[i].labels,
                            vsize * sizeof(*points->points[i].labels));
            }
            points->points[i].labels[j].label_len = strlen(vals);
            points->points[i].labels[j].label =
                malloc(points->points[i].labels[j].label_len + 1);
            snprintf((char *) points->points[i].labels[j].label,
                     points->points[i].labels[j].label_len + 1, "%s", vals);
            vals = strtok_r(NULL, " ", &end_val);
            points->points[i].labels[j].value_len = strlen(vals);
            points->points[i].labels[j].value =
                malloc(points->points[i].labels[j].value_len + 1);
            snprintf((char *) points->points[i].labels[j].value,
                     points->points[i].labels[j].value_len + 1, "%s", vals);
            points->points[i].labels_len++;
            j++;
        }
        points->points_len++;
    }
    return TTS_CLIENT_SUCCESS;
}

static int tts_handle_query(char *line, struct tts_packet *tts_p) {
    TTS_SET_REQUEST_HEADER(tts_p, TTS_QUERY);
    char *token = strtok(line, " ");
    tts_p->query.ts_name_len = strlen(token);
    tts_p->query.ts_name = malloc(tts_p->query.ts_name_len + 1);
    snprintf((char *) tts_p->query.ts_name,
             tts_p->query.ts_name_len + 1, "%s", token);
    while ((token = strtok(NULL, " "))) {
        if (strcmp(token, "*") == 0) {
            tts_p->query.byte = 0x00;
        } else if (strcmp(token, ">") == 0) {
            tts_p->query.bits.major_of = 1;
            token = strtok(NULL, " ");
            tts_p->query.major_of = atoll(token);
            if (get_digits(tts_p->query.major_of) <= 10)
                tts_p->query.major_of *= 1e9;
        } else if (strcmp(token, "<") == 0) {
            tts_p->query.bits.minor_of = 1;
            token = strtok(NULL, " ");
            tts_p->query.minor_of = atol(token);
            if (get_digits(tts_p->query.minor_of) <= 10)
                tts_p->query.minor_of *= 1e9;
        } else if (strcasecmp(token, "range") == 0) {
            tts_p->query.bits.minor_of = 1;
            tts_p->query.bits.major_of = 1;
            token = strtok(NULL, " ");
            tts_p->query.major_of = atoll(token);
            token = strtok(NULL, " ");
            tts_p->query.minor_of = atoll(token);
            if (get_digits(tts_p->query.minor_of) <= 10)
                tts_p->query.minor_of *= 1e9;
            if (get_digits(tts_p->query.major_of) <= 10)
                tts_p->query.major_of *= 1e9;
        } else if (strcasecmp(token, "first") == 0) {
            tts_p->query.bits.first = 1;
        } else if (strcasecmp(token, "last") == 0) {
            tts_p->query.bits.last = 1;
        } else if (strcasecmp(token, "avg") == 0) {
            token = strtok(NULL, " ");
            tts_p->query.mean_val = atoll(token);
            tts_p->query.bits.mean = 1;
        }
    }
    return TTS_CLIENT_SUCCESS;
}

static ssize_t tts_parse_req(char *cmd, char *buf) {
    if (count_tokens(cmd, ' ') < 1)
        return TTS_CLIENT_FAILURE;
    int cmd_id = -1, i, err = 0;
    ssize_t len = 0LL;
    struct tts_packet tts_p;
    memset(&tts_p, 0x00, sizeof(tts_p));
    for (i = 0; i < COMMANDS_NR && cmd_id == -1; ++i) {
        if (strncasecmp(cmd, cmds[i], strlen(cmds[i])) == 0)
            cmd_id = i;
    }
    if (cmd_id == -1)
        goto err;
    remove_newline(cmd);
    err = handlers[cmd_id](cmd + strlen(cmds[cmd_id]), &tts_p);
    if (err < 0)
        goto err;

    len = pack_tts_packet(&tts_p, (uint8_t *) buf);

    return len;
err:
    return TTS_CLIENT_FAILURE;
}

static ssize_t tts_parse_res(char *res, struct tts_packet *tts_p) {
    unpack_tts_packet((uint8_t *) res, tts_p);
    return TTS_CLIENT_SUCCESS;
}

/*
 * Create a non-blocking socket and use it to connect to the specified host and
 * port
 */
static int tts_connect(const struct tts_connect_options *opts) {

    /* socket: create the socket */
    int fd = socket(opts->s_family, SOCK_STREAM, 0);
    if (fd < 0)
        goto err;

    if (opts->s_family == AF_INET) {
        struct sockaddr_in addr;
        struct hostent *server;

        /* gethostbyname: get the server's DNS entry */
        server = gethostbyname(opts->s_addr);
        if (server == NULL)
            goto err;

        /* build the server's address */
        addr.sin_family = opts->s_family;
        addr.sin_port = htons(opts->s_port);
        addr.sin_addr = *((struct in_addr *) server->h_addr);
        bzero(&(addr.sin_zero), 8);

        /* connect: create a connection with the server */
        if (connect(fd, (const struct sockaddr *) &addr, sizeof(addr)) == -1)
            goto err;

    } else if (opts->s_family == AF_UNIX) {
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        if (*opts->s_addr == '\0') {
            *addr.sun_path = '\0';
            strncpy(addr.sun_path+1, opts->s_addr+1, sizeof(addr.sun_path)-2);
        } else {
            strncpy(addr.sun_path, opts->s_addr, sizeof(addr.sun_path)-1);
        }

        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
            goto err;
    }

    return fd;

err:

    if (errno == EINPROGRESS)
        return fd;

    perror("socket(2) opening socket failed");
    return TTS_CLIENT_FAILURE;
}

void tts_client_init(tts_client *client,
                     const struct tts_connect_options * opts) {
    client->buf = malloc(BUFSIZE);
    client->bufsize = 0;
    client->capacity = BUFSIZE;
    client->opts = opts;
}

void tts_client_destroy(tts_client *client) {
    free(client->buf);
}

int tts_client_connect(tts_client *client) {
    int fd = tts_connect(client->opts);
    if (fd < 0)
        return TTS_CLIENT_FAILURE;
    client->fd = fd;
    return TTS_CLIENT_SUCCESS;
}

void tts_client_disconnect(tts_client *client) {
    close(client->fd);
}

int tts_client_send_command(tts_client *client, char *command) {
    ssize_t size = tts_parse_req(command, client->buf);
    if (size == TTS_CLIENT_FAILURE)
        return TTS_CLIENT_FAILURE;
    client->bufsize = size;
    int n = write(client->fd, client->buf, client->bufsize);
    if (n <= 0)
        return TTS_CLIENT_FAILURE;
    return n;
}

int tts_client_recv_response(tts_client *client, struct tts_packet *tts_p) {
    int64_t val;
    uint8_t *ptr = (uint8_t *) client->buf + 1;
    int n = read(client->fd, client->buf, 5);
    unpack_integer(&ptr, 'I', &val);
    if (val == 0)
        goto parse;
    n = read(client->fd, client->buf+5, val);
    if (n <= 0)
        return TTS_CLIENT_FAILURE;
parse:
    tts_parse_res(client->buf, tts_p);
    return n;
}
