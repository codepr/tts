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
#include <stdint.h>

#define TTS_QUERY_ALL_TIMESERIES     0x00
#define TTS_QUERY_ALL_TIMESERIES_AVG 0x01

/*
 * Header type field, separate tts packets into requests and responses, it is
 * required just one bit to represent it
 */
enum {
    TTS_REQUEST  = 0x00,
    TTS_RESPONSE = 0x01
};

/*
 * Header opcode field, describe what command the packet is carrying, there's
 * no real distinction between requests and responses here, although the only
 * 2 valid responses opcode are, as of now, TTS_QUERY_RESPONSE and TTS_ACK.
 *
 * These opcodes can be summarized as
 * - TTS_CREATE_TS      Used to create a new timeseries, refer to
 *                      `struct tts_create_ts`
 * - TTS_DELETE_TS      Used to delete an existing timeseries, refer to
 *                      `struct tts_delete_ts`
 * - TTS_ADDPOINTS      Used to add new (possibly multiple) points to an
 *                      existing timeseries, refer to `struct tts_addpoints`
 * - TTS_QUERY          Used to perform queries and aggregations on an existing
 *                      timeseries such as range queries, means, first, last
 *                      etc. Refer to `struct tts_query`
 * - TTS_QUERY_RESPONSE *Response* Used to return results of a query to a
 *                      connected client
 * - TTS_ACK            *Response* Used as an acknoledgement packet in response
 *                      to an operation request to a connected client
 */
enum {
    TTS_CREATE_TS = 0x00,
    TTS_DELETE_TS,
    TTS_ADDPOINTS,
    TTS_QUERY,
    TTS_QUERY_RESPONSE,
    TTS_ACK
};

/*
 * Header status field, describe the return code of the requested operation, a
 * TTS_ACK responses is entirely contained in just the tts_header
 */
enum {
    TTS_OK = 0x00,
    TTS_ENOTS,       // Not found error, generally a timeseries
    TTS_UNKNOWN_CMD, // Unknown command error
    TTS_EOOM         // Out of memory error
};

/* Helper macros, first argument must be a pointer to a tts_packet struct */
#define TTS_SET_REQUEST_HEADER(r, o) do {   \
    (r)->header.type = TTS_REQUEST;         \
    (r)->header.opcode = (o);               \
} while (0)

#define TTS_SET_RESPONSE_HEADER(r, o, s) do {   \
    (r)->header.type = TTS_RESPONSE;            \
    (r)->header.status = (s);                   \
    (r)->header.opcode = (o);                   \
} while (0)

/* First two mandatory fields on each command */
#define TS_NAME_FIELD    \
    uint8_t ts_name_len; \
    uint8_t *ts_name;    \

/*
 * Simple header to describe the command to be executed, it's formed just by a
 * single byte
 *
 * |   Bit      |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 * |------------|-----------------------------------------------|
 * | Byte 0     | typ |          opcode       |   status  | res.|
 * |____________|_____|_______________________|___________|_____|
 *
 */
union tts_header {
    uint8_t byte;
    struct {
        uint8_t type : 1;
        uint8_t opcode : 4;
        uint8_t status : 2;
        uint8_t reserved : 1;
    };
};

/*
 * Command TTS_CREATE_TS, create a time series defiining it's name and an
 * optional retention member, which represents the maximum difference between
 * the last inserted point and the oldest, 0 means no retetion at all.
 */
struct tts_create_ts {
    TS_NAME_FIELD
    uint32_t retention;  // unused
};

/*
 * Command TTS_DELETE, destroy a time series represented by a name
 */
struct tts_delete_ts {
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
    uint16_t points_len;
    struct {
        union {
            uint8_t byte;
            struct {
                uint8_t ts_sec_set : 1;
                uint8_t ts_nsec_set : 1;
                uint8_t reserved : 6;
            } bits;
        };
        long double value;
        uint64_t ts_sec;
        uint64_t ts_nsec;
        uint16_t labels_len;
        struct {
            uint16_t label_len;
            uint8_t *label;
            uint16_t value_len;
            uint8_t *value;
        } *labels;
    } *points;
};

/*
 * Command TTS_QUERY, generic query command to retrieve one or more points,
 * depending on the flag passed in, in different forms:
 *
 * - MEAN
 * - FIRST
 * - LAST
 * - <TBD>
 *
 * Proceeding in incremental improvements, for now we just accept seconds as
 * values on mean_val, major_of, minor_of.
 *
 * TODO Add tags field, to be indexed, probably in a hashmap
 */
struct tts_query {
    TS_NAME_FIELD
    union {
        uint8_t byte;
        struct {
            uint8_t mean : 1;
            uint8_t first : 1;
            uint8_t last : 1;
            uint8_t major_of : 1;
            uint8_t minor_of : 1;
            uint8_t reserved : 3;
        } bits;
    };
    uint64_t mean_val;   // present only if mean = 1
    uint64_t major_of;   // present only if major_of = 1
    uint64_t minor_of;   // present only if minor_of = 1
};

/*
 * An ACK response for a query request, it carries an array of tuples with
 * return codes and optional values as part of the result of the query issued
 */
struct tts_query_response {
    uint64_t len;
    struct {
        uint8_t rc;
        uint64_t ts_sec;
        uint64_t ts_nsec;
        long double value;
        uint16_t labels_len;
        struct {
            uint16_t label_len;
            uint8_t *label;
            uint16_t value_len;
            uint8_t *value;
        } *labels;
    } *results;
};

/*
 * Generic TTS packet, it can contains requests or responses, based on the
 * header opcode, just a union of previously defined structures
 *
 * Can be summarized as the following representation
 *
 * |   Bit      |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 * |------------|-----------------------------------------------|
 * | Byte 0     | typ |          opcode       |   status  | res.|
 * |____________|_____|_______________________|___________|_____|
 * | Byte 1     |           Packet length word1 MSB             |
 * | Byte 2     |           Packet length word1 LSB             |
 * | Byte 3     |           Packet length word2 MSB             |
 * | Byte 4     |           Packet length word2 LSB             |
 * |------------|-----------------------------------------------|
 * |  ...                           ...                         |
 *
 * The first 5 bytes contain the header of the packet and the remaning length
 * to read to complete a packet
 */
struct tts_packet {
    union tts_header header;
    uint32_t len;
    union {
        struct tts_create_ts create;
        struct tts_delete_ts drop;  // just to avoid keyword naming clash
        struct tts_addpoints addpoints;
        struct tts_query query;
        struct tts_query_response query_r;
    };
};

void unpack_tts_packet(uint8_t *, struct tts_packet *);
ssize_t pack_tts_packet(const struct tts_packet *, uint8_t *);
void tts_packet_destroy(struct tts_packet *);

#endif
