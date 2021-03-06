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

#ifndef TTS_CLIENT_H
#define TTS_CLIENT_H

#include <stdio.h>
#include <netdb.h>

#define TTS_CLIENT_SUCCESS       0
#define TTS_CLIENT_FAILURE      -1
#define TTS_CLIENT_UNKNOWN_CMD  -2

struct tts_packet;

typedef struct tts_client tts_client;

/*
 * Connection options, use this structure to specify connection related opts
 * like socket family, host port and timeout for communication
 */
struct tts_connect_options {
    int timeout;
    int s_family;
    int s_port;
    char *s_addr;
};

/*
 * Pretty basic connection wrapper, just a FD with a buffer tracking bytes and
 * some options for connection
 */
struct tts_client {
    int fd;
    const struct tts_connect_options *opts;
    size_t bufsize;
    size_t capacity;
    char *buf;
};

void tts_client_init(tts_client *, const struct tts_connect_options *);
void tts_client_destroy(tts_client *);
int tts_client_connect(tts_client *);
void tts_client_disconnect(tts_client *);
int tts_client_send_command(tts_client *, char *);
int tts_client_recv_response(tts_client *, struct tts_packet *);
void tts_client_packet_destroy(struct tts_packet *);

#endif
