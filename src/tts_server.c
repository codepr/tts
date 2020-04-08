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
#define EV_SOURCE
#define EV_TCP_SOURCE
#include "ev_tcp.h"
#include "tts_log.h"
#include "tts_server.h"
#include "tts_handlers.h"
#include "tts_protocol.h"

#define BACKLOG 128

struct tts_server tts_server;

static void on_close(ev_tcp_handle *client, int err) {
    (void) client;
    if (err == EV_TCP_SUCCESS)
        log_debug("Closed connection with %s:%i", client->addr, client->port);
    else
        log_debug("Connection closed: %s", ev_tcp_err(err));
    free(client);
}

static void on_write(ev_tcp_handle *client) {
    (void) client;
    log_debug("Written response");
}

static void on_data(ev_tcp_handle *client) {
    struct tts_payload payload = {
        .buf = &client->buffer,
        .tts_db = tts_server.db
    };
    unpack_tts_packet((uint8_t *) client->buffer.buf, &payload.packet);
    int err = tts_handle_packet(&payload);
    if (err == TTS_OK)
        ev_tcp_enqueue_write(client);
    tts_packet_destroy(&payload.packet);
}

static void on_connection(ev_tcp_handle *server) {
    int err = 0;
    ev_tcp_handle *client = malloc(sizeof(*client));
    if ((err = ev_tcp_server_accept(server, client, on_data, on_write)) < 0) {
            log_error("Error occured: %s",
                      err == -1 ? strerror(errno) : ev_tcp_err(err));
        free(client);
    } else {
        log_debug("New connection from %s:%i", client->addr, client->port);
        ev_tcp_handle_set_on_close(client, on_close);
    }
}

static void ts_destroy(struct tts_timeseries *tss) {
    struct tts_timeseries *ts, *tmp;
    HASH_ITER(hh, tss, ts, tmp) {
        HASH_DEL(tss, ts);
        TTS_VECTOR_DESTROY(ts->timestamps);
        struct tts_record *records = NULL;
        for (size_t i = 0; i < TTS_VECTOR_SIZE(ts->columns); ++i) {
            records = TTS_VECTOR_AT(ts->columns, i);
            for (size_t j = 0; j < ts->fields_nr; ++j) {
                free(records[j].field);
                free(records[j].value);
            }
            free(records);
        }
        TTS_VECTOR_DESTROY(ts->columns);
        free(ts);
    }
}

int tts_start_server(const char *host, int port) {
    tts_server.db = malloc(sizeof(struct tts_database));
    tts_server.db->timeseries = NULL;
    ev_context *ctx = ev_get_ev_context();
    ev_tcp_server server;
    ev_tcp_server_init(&server, ctx, BACKLOG);
    int err = ev_tcp_server_listen(&server, host, port, on_connection);
    if (err < 0) {
        if (err == -1)
            log_fatal("Error occured: %s\n", strerror(errno));
        else
            log_fatal("Error occured: %s\n", ev_tcp_err(err));
    }

    log_debug("Listening on %s:%i", host, port);

    // Blocking call
    ev_tcp_server_run(&server);

    // This could be registered to a SIGINT|SIGTERM signal notification
    // to stop the server with Ctrl+C
    ev_tcp_server_stop(&server);

    ts_destroy(tts_server.db->timeseries);
    free(tts_server.db);

    return 0;
}
