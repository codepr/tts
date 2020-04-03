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

#include <stdlib.h>
#include <string.h>
#include "pack.h"
#include "tts_protocol.h"

/*
 * Unpack from binary to struct tts_create packet, the form of the packet is
 * pretty simple, we just proceed populating each member with its value from
 * the buffer, starting right after the header byte and the size:
 *
 * |   Bit      |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 * |------------|-----------------------------------------------|
 * | Byte 5     |          Time series name len MSB             |
 * | Byte 6     |          Time series name len LSB             |
 * |------------|-----------------------------------------------|
 * | Byte 7     |                                               |
 * |   .        |              Time series name                 |
 * | Byte N     |                                               |
 * |------------|-----------------------------------------------|<- Array start
 * | Byte N+1   |                Field len MSB                  |
 * | Byte N+2   |                Field len LSB                  |
 * |------------|-----------------------------------------------|
 * | Byte N+3   |                                               |
 * |   .        |                  Field name                   |
 * | Byte N+M   |                                               |
 * |____________|_______________________________________________|
 *
 * The last two steps, will be repeated until the expected length of the packet
 * is exhausted.
 */
static void unpack_tts_create(uint8_t *buf, size_t len, struct tts_create *c) {
    int64_t val = 0;
    // zero'ing tts_create struct
    memset(c, 0x00, sizeof(*c));
    unpack_integer(&buf, 'B', &val);
    c->ts_name_len = val;
    c->ts_name = malloc(c->ts_name_len + 1);
    unpack_bytes(&buf, c->ts_name_len, c->ts_name);
    len -= sizeof(uint8_t) + c->ts_name_len;
    for (int i = 0; len > 0; ++i) {
        /*
         * Probably not the best way to handle it but we have to make room for
         * additional incoming tuples
         */
        c->fields = realloc(c->fields, (i + 1) * sizeof(*c->fields));
        unpack_integer(&buf, 'H', &val);
        c->fields[i].field_len = val;
        c->fields[i].field = malloc(c->fields[i].field_len + 1);
        unpack_bytes(&buf, c->fields[i].field_len, c->fields[i].field);
        len -= sizeof(uint16_t) + c->fields[i].field_len;
        ++c->fields_len;
    }
}

/*
 * Unpack from binary to struct tts_delete packet, the form of the packet is
 * the most simple (excluding the ACK which we don't expect to unpack as of
 * now)
 *
 * |   Bit      |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 * |------------|-----------------------------------------------|
 * | Byte 5     |          Time series name len MSB             |
 * | Byte 6     |          Time series name len LSB             |
 * |------------|-----------------------------------------------|
 * | Byte 7     |                                               |
 * |   .        |              Time series name                 |
 * | Byte N     |                                               |
 * |____________|_______________________________________________|
 */
static void unpack_tts_delete(uint8_t *buf, size_t len, struct tts_delete *d) {
    (void) len;
    int64_t val = 0;
    // zero'ing tts_delete struct
    memset(d, 0x00, sizeof(*d));
    unpack_integer(&buf, 'B', &val);
    d->ts_name_len = val;
    d->ts_name = malloc(d->ts_name_len + 1);
    unpack_bytes(&buf, d->ts_name_len, d->ts_name);
}

/*
 * Unpack from binary to struct tts_addpoints packet, the form of the packet is
 * the following, not that different from the previous
 *
 * |   Bit      |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 * |------------|-----------------------------------------------|
 * | Byte 5     |          Time series name len MSB             |
 * | Byte 6     |          Time series name len LSB             |
 * |------------|-----------------------------------------------|
 * | Byte 7     |                                               |
 * |   .        |              Time series name                 |
 * | Byte N     |                                               |
 * |------------|-----------------------------------------------|<- Array start
 * | Byte N+1   |                Field len MSB                  |
 * | Byte N+2   |                Field len LSB                  |
 * |------------|-----------------------------------------------|
 * | Byte N+3   |                                               |
 * |   .        |                 Field name                    |
 * | Byte N+M   |                                               |
 * |------------|-----------------------------------------------|
 * | Byte K     |                Value len MSB                  |
 * | Byte K+1   |                Value len LSB                  |
 * |------------|-----------------------------------------------|
 * | Byte K+2   |                                               |
 * |   .        |          Timestamp seconds component          |
 * | Byte K+10  |                                               |
 * |------------|-----------------------------------------------|
 * | Byte K+11  |                                               |
 * |   .        |        Timestamp nanoseconds component        |
 * | Byte K+19  |                                               |
 * |____________|_______________________________________________|
 *
 * The steps starting at [Array start] will be repeated until the expected
 * length of the packet is exhausted.
 */
static void unpack_tts_addpoints(uint8_t *buf, size_t len,
                                 struct tts_addpoints *a) {
    int64_t val = 0;
    // zero'ing tts_addpoints struct
    memset(a, 0x00, sizeof(*a));
    unpack_integer(&buf, 'B', &val);
    a->ts_name_len = val;
    a->ts_name = malloc(a->ts_name_len + 1);
    unpack_bytes(&buf, a->ts_name_len, a->ts_name);
    len -= sizeof(uint8_t) + a->ts_name_len;
    for (int i = 0; len > 0; ++i) {
        a->points = realloc(a->points, (i + 1) * sizeof(*a->points));
        unpack_integer(&buf, 'B', &val);
        printf("tts_addpoints.points[%i].ts_flags.byte %li\n", i, val);
        a->points[i].ts_flags.byte = val;
        // Unpack the seconds and nanoseconds component of the timestamp if
        // present
        if (a->points[i].ts_flags.bits.ts_sec_set == 1) {
            unpack_integer(&buf, 'Q', &val);
            a->points[i].ts_sec = val;
            len -= sizeof(uint64_t);
        }
        if (a->points[i].ts_flags.bits.ts_nsec_set == 1) {
            unpack_integer(&buf, 'Q', &val);
            a->points[i].ts_nsec = val;
            len -= sizeof(uint64_t);
        }
        unpack_integer(&buf, 'H', &val);
        a->points[i].values_len = val;
        len -= sizeof(uint16_t);
        a->points[i].values =
            calloc(a->points[i].values_len, sizeof(*a->points[i].values));
        for (int j = 0; j < a->points[i].values_len; ++j) {
            unpack_integer(&buf, 'H', &val);
            printf("tts_addpoints.points[%i].field_len %li\n", i, val);
            a->points[i].values[j].field_len = val;
            a->points[i].values[j].field = malloc(val + 1);
            unpack_bytes(&buf, a->points[i].values[j].field_len,
                         a->points[i].values[j].field);
            unpack_integer(&buf, 'H', &val);
            printf("tts_addpoints.points[%i].value_len %li\n", i, val);
            a->points[i].values[j].value_len = val;
            a->points[i].values[j].value = malloc(val + 1);
            unpack_bytes(&buf, a->points[i].values[j].value_len,
                         a->points[i].values[j].value);
            // Update the length remaining after the unpack of the field + value
            // bytes
            len -= sizeof(uint8_t) + sizeof(uint16_t) * 2 +
                a->points[i].values[j].field_len +
                a->points[i].values[j].value_len;
        }
        ++a->points_len;
    }
}

/*
 * Unpack from binary to struct tts_query packet:
 *
 * |   Bit      |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 * |------------|-----------------------------------------------|
 * | Byte 5     | avg |first| last|  gt |  lt |    reserved     |
 * |------------|-----------------------------------------------|
 * | Byte 6     |          Time series name len MSB             |
 * | Byte 7     |          Time series name len LSB             |
 * |------------|-----------------------------------------------|
 * | Byte 8     |                                               |
 * |   .        |              Time series name                 |
 * | Byte N     |                                               |
 * |------------|-----------------------------------------------|
 * | Byte N+1   |                                               |
 * |   .        |           Mean value (if avg == 1)            |
 * | Byte N+7   |                                               |
 * |------------|-----------------------------------------------|
 * | Byte N+8   |                                               |
 * |   .        |          First value (if first == 1)          |
 * | Byte N+15  |                                               |
 * |------------|-----------------------------------------------|
 * | Byte N+16  |                                               |
 * |   .        |        Major-of value (if major_of == 1)      |
 * | Byte N+23  |                                               |
 * |------------|-----------------------------------------------|
 * | Byte N+24  |                                               |
 * |   .        |        Minor-of value (if minor_of == 1)      |
 * | Byte N+31  |                                               |
 * |____________|_______________________________________________|
 */
static void unpack_tts_query(uint8_t *buf, size_t len, struct tts_query *q) {
    int64_t val = 0;
    // zero'ing tts_query struct
    memset(q, 0x00, sizeof(*q));
    unpack_integer(&buf, 'B', &val);
    q->byte = val;
    printf("q->byte %u\n", q->byte);
    unpack_integer(&buf, 'B', &val);
    q->ts_name_len = val;
    printf("q->ts_name_len %u\n", q->ts_name_len);
    q->ts_name = malloc(q->ts_name_len + 1);
    unpack_bytes(&buf, q->ts_name_len, q->ts_name);
    len -= sizeof(uint8_t) * 2 + q->ts_name_len;
    if (q->bits.mean == 1) {
        unpack_integer(&buf, 'Q', (int64_t *) &q->mean_val);
        len -= sizeof(uint64_t);
    }
    if (q->bits.major_of == 1) {
        unpack_integer(&buf, 'Q', (int64_t *) &q->major_of);
        len -= sizeof(uint64_t);
    }
    if (q->bits.minor_of == 1) {
        unpack_integer(&buf, 'Q', (int64_t *) &q->minor_of);
        len -= sizeof(uint64_t);
    }
}

/*
 * Unpack a tts_packet, after reading the header opcode and the length of the
 * entire packet, calls the right unpack function based on the command type
 */
void unpack_tts_packet(uint8_t *buf, struct tts_packet *tts_p) {
    size_t len = 0;
    int64_t val = 0;
    tts_p->header.byte = *buf++;
    unpack_integer(&buf, 'I', &val);
    len = val;
    printf("Packet len %lu\n", len);
    switch(tts_p->header.byte) {
        case TTS_CREATE:
            unpack_tts_create(buf, len, &tts_p->create);
            break;
        case TTS_DELETE:
            unpack_tts_delete(buf, len, &tts_p->drop);
            break;
        case TTS_ADDPOINTS:
            unpack_tts_addpoints(buf, len, &tts_p->addpoints);
            break;
        case TTS_QUERY:
            unpack_tts_query(buf, len, &tts_p->query);
            break;
    }
}

/*
 * Simple helper to set a bytes buffer to an ACK response with a defined RC
 */
uint64_t pack_tts_ack(uint8_t *buf, int rc) {
    pack_integer(&buf, 'B', TTS_ACK);
    pack_integer(&buf, 'B', rc);
    return 2;
}

uint64_t pack_tts_query_ack(uint8_t *buf, const struct tts_query_ack *qa) {
    size_t len = 0LL;
    size_t packed = 0LL;
    uint8_t tmp[2048], *ptr = &tmp[0];
    for (uint64_t i = 0; i < qa->len; ++i) {
        printf("Packing point\n");
        packed = pack(ptr, "BHsHsQQ",
                      qa->results[i].rc,
                      qa->results[i].field_len,
                      qa->results[i].field,
                      qa->results[i].value_len,
                      qa->results[i].value,
                      qa->results[i].ts_sec,
                      qa->results[i].ts_nsec);
        len += packed;
        ptr += packed;
        packed = 0;
    }
    len += pack(buf, "BI", TTS_QUERY_RESPONSE, (uint32_t) len);
    memcpy(buf + 5, tmp, len - 5);
    return len;
}

void tts_packet_destroy(struct tts_packet *packet) {
    switch (packet->header.byte) {
        case TTS_CREATE:
            free(packet->create.ts_name);
            for (int i = 0; i < packet->create.fields_len; ++i)
                free(packet->create.fields[i].field);
            free(packet->create.fields);
            break;
        case TTS_DELETE:
            free(packet->drop.ts_name);
            break;
        case TTS_ADDPOINTS:
            free(packet->addpoints.ts_name);
            for (int i = 0; i < packet->addpoints.points_len; ++i) {
                free(packet->addpoints.points[i].values);
                //free(packet->addpoints.points[i].field);
                //free(packet->addpoints.points[i].value);
            }
            free(packet->addpoints.points);
            break;
        case TTS_QUERY:
            free(packet->query.ts_name);
            break;
    }
}
