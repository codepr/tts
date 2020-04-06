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

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "pack.h"
#include "tts_protocol.h"
#include "tts_client.h"

#define COMMANDS_NR 4

typedef int (*tts_cmd_handler)(char *, struct tts_packet *);

static int tts_handle_new(char *, struct tts_packet *);
static int tts_handle_del(char *, struct tts_packet *);
static int tts_handle_add(char *, struct tts_packet *);
static int tts_handle_query(char *, struct tts_packet *);
//static void tts_handle_ack(char *);
//static void tts_handle_query_response(char *);

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

static inline void strip_spaces(char **str) {
    if (!*str) return;
    while (isspace(**str) && **str) ++(*str);
}

static inline void remove_newline(char *str) {
    char *ptr = strstr(str, "\n");
    *ptr = '\0';
}

static int tts_handle_new(char *line, struct tts_packet *tts_p) {
    tts_p->header.byte = TTS_CREATE;
    struct tts_create *create = &tts_p->create;
    char *token = strtok(line, " ");
    create->ts_name_len = strlen(token);
    create->ts_name = malloc(create->ts_name_len + 1);
    snprintf((char *) create->ts_name, create->ts_name_len + 1, "%s", token);
    for (int j = 0; (token = strtok(NULL, " ")); ++j) {
        create->fields = realloc(create->fields,
                                 (j + 1) * sizeof(*create->fields));
        create->fields[j].field_len = strlen(token);
        create->fields[j].field = malloc(create->fields[j].field_len + 1);
        snprintf((char *) create->fields[j].field,
                 create->fields[j].field_len + 1, "%s", token);
        create->fields_len++;
    }
    return 0;
}

static int tts_handle_del(char *line, struct tts_packet *tts_p) {
    tts_p->header.byte = TTS_DELETE;
    struct tts_delete *drop = &tts_p->drop;
    char *token = strtok(line, " ");
    drop->ts_name_len = strlen(token);
    drop->ts_name = malloc(drop->ts_name_len + 1);
    snprintf((char *) drop->ts_name, drop->ts_name_len + 1, "%s", token);
    return 0;
}

static int tts_handle_add(char *line, struct tts_packet *tts_p) {
    tts_p->header.byte = TTS_ADDPOINTS;
    struct tts_addpoints *points = &tts_p->addpoints;
    char *end_str, *end_val, *vals = NULL;
    char *token = strtok_r(line, " ", &end_str);
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
        points->points[i].values =
            calloc(vsize, sizeof(*points->points[i].values));
        j = 0;
        vals = strtok_r(token, " ", &end_val);
        do {
            if (j == vsize - 1) {
                vsize *= 2;
                points->points[i].values =
                    realloc(points->points[i].values,
                            vsize * sizeof(*points->points[i].values));
            }
            points->points[i].values[j].field_len = strlen(vals);
            points->points[i].values[j].field =
                malloc(points->points[i].values[j].field_len + 1);
            snprintf((char *) points->points[i].values[j].field,
                     points->points[i].values[j].field_len + 1, "%s", vals);
            vals = strtok_r(NULL, " ", &end_val);
            points->points[i].values[j].value_len = strlen(vals);
            points->points[i].values[j].value =
                malloc(points->points[i].values[j].value_len + 1);
            snprintf((char *) points->points[i].values[j].value,
                     points->points[i].values[j].value_len + 1, "%s", vals);
            points->points[i].values_len++;
            j++;
        } while ((vals = strtok_r(NULL, " ", &end_val)));
        points->points_len++;
        points->points[i].ts_flags.bits.ts_sec_set = 0;
        points->points[i].ts_flags.bits.ts_nsec_set = 0;
    }
    return 0;
}

static int tts_handle_query(char *line, struct tts_packet *tts_p) {
    tts_p->header.byte = TTS_QUERY;
    char *token = strtok(line, " ");
    tts_p->query.ts_name_len = strlen(token);
    tts_p->query.ts_name = malloc(tts_p->query.ts_name_len + 1);
    snprintf((char *) tts_p->query.ts_name,
             tts_p->query.ts_name_len + 1, "%s", token);
    tts_p->query.byte = 0x00;
    return 0;
}

static ssize_t tts_parse_req(char *cmd, char *buf) {
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
    printf("Unknown command\n");
    return -1;
}

static ssize_t tts_parse_res(char *res, struct tts_packet *tts_p) {
    unpack_tts_packet((uint8_t *) res, tts_p);
    return 0;
}

/*
 * Create a non-blocking socket and use it to connect to the specified host and
 * port
 */
static int tts_connect(const char *host, int port) {

    struct sockaddr_in serveraddr;
    struct hostent *server;

    /* socket: create the socket */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        goto err;

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(host);
    if (server == NULL)
        goto err;

    /* build the server's address */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr = *((struct in_addr *) server->h_addr);
    bzero(&(serveraddr.sin_zero), 8);

    /* connect: create a connection with the server */
    if (connect(fd, (const struct sockaddr *) &serveraddr,
                sizeof(serveraddr)) < 0)
        goto err;

    return fd;

err:

    if (errno == EINPROGRESS)
        return fd;

    perror("socket(2) opening socket failed");
    return -1;
}

void tts_client_init(tts_client *client, const char *host, int port) {
    client->buf = malloc(2048);
    client->bufsize = 0;
    client->capacity = 2048;
    client->host = (char *) host;
    client->port = port;
}

void tts_client_destroy(tts_client *client) {
    free(client->buf);
}

int tts_client_connect(tts_client *client) {
    int fd = tts_connect(client->host, client->port);
    if (fd < 0)
        return -1;
    client->fd = fd;
    return 0;
}

void tts_client_disconnect(tts_client *client) {
    close(client->fd);
}

int tts_client_send_command(tts_client *client, char *command) {
    client->bufsize = tts_parse_req(command, client->buf);
    int n = write(client->fd, client->buf, client->bufsize);
    if (n <= 0)
        return -1;
    return n;
}

int tts_client_recv_response(tts_client *client, struct tts_packet *tts_p) {
    int64_t val;
    uint8_t *ptr = (uint8_t *) client->buf + 1;
    int n = read(client->fd, client->buf, 5);
    unpack_integer(&ptr, 'I', &val);
    n = read(client->fd, client->buf+5, val);
    if (n <= 0)
        return -1;
    tts_parse_res(client->buf, tts_p);
    return n;
}
