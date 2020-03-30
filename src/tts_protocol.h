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

#ifndef TTS_PROTOCOL_H
#define TTS_PROTOCOL_H

#include "tts.h"

// Return codes for ACK response
#define OK  0
#define NOK 1

// Base command bytes, draft
#define TTS_CREATE         0x00
#define TTS_DELETE         0x01
#define TTS_ADDPOINTS      0x02
#define TTS_QUERY          0x03
#define TTS_ACK            0x04
#define TTS_QUERY_RESPONSE 0x05

/* First two mandatory fields on each command */
#define TS_NAME_FIELD                              \
    unsigned char ts_name_len;                     \
    unsigned char ts_name[TTS_TS_NAME_MAX_LENGTH]; \

/*
 * Simple header to describe the command to be executed, it's formed just by a
 * single byte
 */
struct tts_header {
    unsigned char byte;
};

/*
 * Command TTS_CREATE, create a time series defiining it's name and a list of
 * fields, like a dead-simple schema for the columns of the series
 */
struct tts_create {
    TS_NAME_FIELD
    struct {
        unsigned short field_len;
        unsigned char *field;
    } fields;
};

/*
 * Command TTS_DELETE, destroy a time series represented by a name
 */
struct tts_delete {
    TS_NAME_FIELD
};

/*
 * Command TTS_ADDPOINTS, it's the main insertion command, it carries the name
 * of the time-series where the new values will be stored, a list of
 * field-value to be stored and optionally a timestamp. If the timestamp is not
 * present, the UTC now will be inserted instead.
 *
 * TODO Add tags field, to be indexed, probably in a hashmap
 */
struct tts_addpoints {
    TS_NAME_FIELD
    struct {
        unsigned short field_len;
        unsigned char *field;
        unsigned short value_len;
        unsigned char *value;
        unsigned long timestamp;
    } points;
};

/*
 * Command TTS_QUERY, generic query command to retrieve one or more points,
 * depending on the flag passed in, in different forms:
 * - MEAN
 * - FIRST
 * - LAST
 * - <TBD>
 *
 * TODO Add tags field, to be indexed, probably in a hashmap
 */
struct tts_query {
    union {
        unsigned char byte;
        struct {
            unsigned mean : 1;
            unsigned first : 1;
            unsigned last : 1;
            unsigned major_of : 1;
            unsigned minor_of : 1;
            unsigned reserved : 3;
        } bits;
    };
    TS_NAME_FIELD
    unsigned short field_len;
    unsigned char *field;
    unsigned long mean_val;   // present only if mean = 1
    unsigned long major_of;   // present only if major_of = 1
    unsigned long minor_of;   // present only if minor_of = 1
};

/*
 * Just an ACK response, stating the outcome of the operation requested. Return
 * code can be either an OK and a NOK for now, without authentication and
 * restrictions of sort, it's enough.
 */
struct tts_ack {
    unsigned char rc;
};

/*
 * An ACK response for a query request, it carries an array of tuples with
 * return codes and optional values as part of the result of the query issued
 */
struct tts_query_ack {
    struct {
        unsigned char rc;
        unsigned short field_len;
        unsigned char *field;
        unsigned short value_len;
        unsigned char *value;
        unsigned long ts_sec;
        unsigned long ts_nsec;
    } results;
};

/*
 * Generic TTS packet, it can contains requests or responses, based on the
 * header opcode, just a union of previously defined structures
 */
struct tts_packet {
    struct tts_header header;
    union {
        struct tts_create create;
        struct tts_delete drop;  // just to avoid keyword naming clash
        struct tts_addpoints addpoints;
        struct tts_query query;
        struct tts_ack ack;
        struct tts_query_ack query_ack;
    };
};

#endif
