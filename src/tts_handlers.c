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

int tts_handle_tts_create(struct tts_payload *payload) {
    ev_tcp_handle *handle = payload->handle;
    struct tts_packet *packet = &payload->packet;
    printf("Time series %s\n", packet->create.ts_name);
    printf("Fields nr: %u\n", packet->create.fields_len);
    printf("Fields: ");
    for (int i = 0; i < packet->create.fields_len; i++)
        printf("%s ", packet->create.fields[i].field);
    printf("\n");
    struct tts_timeseries *ts = malloc(sizeof(*ts));
    TTS_TIMESERIES_INIT(ts, packet->create.ts_name, packet->create.fields_len);
    for (int i = 0; i < packet->create.fields_len; i++)
        TTS_VECTOR_APPEND(ts->fields, (char *) packet->create.fields[i].field);
    // add to db hashmap
    HASH_ADD_STR(payload->tts_db->timeseries, name, ts);
    handle->buffer.size = pack_tts_ack((uint8_t *) handle->buffer.buf, TTS_OK);
    return TTS_OK;
}

int tts_handle_tts_delete(struct tts_payload *payload) {
    ev_tcp_handle *handle = payload->handle;
    struct tts_packet *packet = &payload->packet;
    printf("Time series %s\n", packet->drop.ts_name);
    struct tts_timeseries *ts = NULL;
    char *key = (char *) packet->drop.ts_name;
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    if (!ts) {
        handle->buffer.size =
            pack_tts_ack((uint8_t *) handle->buffer.buf, TTS_NOK);
    } else {
        HASH_DEL(payload->tts_db->timeseries, ts);
        handle->buffer.size =
            pack_tts_ack((uint8_t *) handle->buffer.buf, TTS_OK);
    }
    return TTS_OK;
}

int tts_handle_tts_addpoints(struct tts_payload *payload) {
    ev_tcp_handle *handle = payload->handle;
    struct tts_packet *packet = &payload->packet;
    printf("Time series %s\n", packet->addpoints.ts_name);
    printf("Points:\n");
    for (int i = 0; i < packet->addpoints.points_len; i++)
        for (int j = 0; j < packet->addpoints.points[i].values_len; j++)
        printf("%s: %s %lu %lu\n", packet->addpoints.points[i].values[j].field,
               packet->addpoints.points[i].values[j].value,
               packet->addpoints.points[i].ts_sec,
               packet->addpoints.points[i].ts_nsec);
    struct tts_timeseries *ts = NULL;
    char *key = (char *) packet->addpoints.ts_name;
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    if (!ts) {
        handle->buffer.size =
            pack_tts_ack((uint8_t *) handle->buffer.buf, TTS_NOK);
    } else {
        for (int i = 0; i < packet->addpoints.points_len; i++) {
            struct timespec timestamp = {
                .tv_sec = packet->addpoints.points[i].ts_sec,
                .tv_nsec = packet->addpoints.points[i].ts_nsec
            };
            TTS_VECTOR_APPEND(ts->timestamps, timestamp);
            struct tts_record *records =
                malloc(ts->fields_nr * sizeof(struct tts_record));
            for (int j = 0; j < packet->addpoints.points[i].values_len; ++j) {
                records[j].field =
                    (char *) packet->addpoints.points[i].values[j].field;
                records[j].value = packet->addpoints.points[i].values[j].value;
            }
            TTS_VECTOR_APPEND(ts->columns, records);
        }
        handle->buffer.size =
            pack_tts_ack((uint8_t *) handle->buffer.buf, TTS_OK);
    }
    return TTS_OK;
}

int tts_handle_tts_query(struct tts_payload *payload) {
    ev_tcp_handle *handle = payload->handle;
    struct tts_packet *packet = &payload->packet;
    printf("Time series %s\n", packet->query.ts_name);
    struct tts_timeseries *ts = NULL;
    char *key = (char *) packet->query.ts_name;
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    if (!ts) {
        handle->buffer.size =
            pack_tts_ack((uint8_t *) handle->buffer.buf, TTS_NOK);
    } else {
        if (packet->query.byte == 0x00) {
            size_t colsize = TTS_VECTOR_SIZE(ts->columns);
            struct tts_query_ack qa;
            qa.results = calloc(colsize, sizeof(*qa.results));
            struct tts_record *record;
            struct timespec *t;
            qa.len = colsize;
            for (size_t i = 0; i < colsize; ++i) {
                t = &TTS_VECTOR_AT(ts->timestamps, i);
                record = TTS_VECTOR_AT(ts->columns, i);
                qa.results[i].rc = TTS_OK;
                qa.results[i].field_len = strlen(record->field);
                qa.results[i].field = (uint8_t *) record->field;
                qa.results[i].value_len = strlen(record->value);
                qa.results[i].value = record->value;
                qa.results[i].ts_sec = t->tv_sec;
                qa.results[i].ts_nsec = t->tv_nsec;
            }
            handle->buffer.size =
                pack_tts_query_ack((uint8_t *) handle->buffer.buf, &qa);
        }
    }
    return TTS_OK;
}

int tts_handle_packet(struct tts_payload *payload) {
    int rc = 0;
    switch (payload->packet.header.byte) {
        case TTS_CREATE:
            rc = tts_handle_tts_create(payload);
            break;
        case TTS_DELETE:
            rc = tts_handle_tts_delete(payload);
            break;
        case TTS_ADDPOINTS:
            rc = tts_handle_tts_addpoints(payload);
            break;
        case TTS_QUERY:
            rc = tts_handle_tts_query(payload);
            break;
    }
    return rc;
}
