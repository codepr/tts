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
#include "tts_protocol.h"
#include "tts_handlers.h"

int tts_handle_tts_create(struct tts_packet *packet, ev_tcp_handle *handle) {
    (void) handle;
    printf("Time series %s\n", packet->create.ts_name);
    printf("Fields nr: %u\n", packet->create.fields_len);
    printf("Fields: ");
    for (int i = 0; i < packet->create.fields_len; i++)
        printf("%s ", packet->create.fields[i].field);
    printf("\n");
    return 0;
}

int tts_handle_tts_delete(struct tts_packet *packet, ev_tcp_handle *handle) {
    (void) handle;
    printf("Time series %s\n", packet->drop.ts_name);
    return 0;
}

int tts_handle_tts_addpoints(struct tts_packet *packet, ev_tcp_handle *handle) {
    (void) handle;
    printf("Time series %s\n", packet->addpoints.ts_name);
    printf("Points:\n");
    for (int i = 0; i < packet->addpoints.points_len; i++)
        printf("%s: %s %lu %lu\n", packet->addpoints.points[i].field,
               packet->addpoints.points[i].value,
               packet->addpoints.points[i].ts_sec,
               packet->addpoints.points[i].ts_nsec);
    return 0;
}

int tts_handle_tts_query(struct tts_packet *packet, ev_tcp_handle *handle) {
    (void) handle;
    printf("Time series %s\n", packet->query.ts_name);
    return 0;
}

int tts_handle_packet(struct tts_packet *packet, ev_tcp_handle *handle) {
    switch (packet->header.byte) {
        case TTS_CREATE:
            tts_handle_tts_create(packet, handle);
            break;
        case TTS_DELETE:
            tts_handle_tts_delete(packet, handle);
            break;
        case TTS_ADDPOINTS:
            tts_handle_tts_addpoints(packet, handle);
            break;
        case TTS_QUERY:
            tts_handle_tts_query(packet, handle);
            break;
    }
    return 0;
}
