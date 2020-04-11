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
#include <sys/types.h>
#include "pack.h"
#include "tts_protocol.h"

/*
 * ========================
 *   UNPACKING FUNCTONS
 *========================
 */

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
static void unpack_tts_create(uint8_t *buf, size_t len,
                              struct tts_create_ts *c) {
    int64_t val = 0;
    // zero'ing tts_create struct
    memset(c, 0x00, sizeof(*c));
    len -= unpack_integer(&buf, 'B', &val);
    c->ts_name_len = val;
    c->ts_name = malloc(c->ts_name_len + 1);
    len -= unpack_bytes(&buf, c->ts_name_len, c->ts_name);
    len -= unpack_integer(&buf, 'I', (int64_t *) &c->retention);
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
static void unpack_tts_delete(uint8_t *buf, size_t len,
                              struct tts_delete_ts *d) {
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
 * | Byte N+1   | tss | tsn |           reserved                |
 * |------------|-----------------------------------------------|
 * | Byte N+2   |                                               |
 * |   .        |                  value                        |
 * | Byte N+18  |                                               |
 * |------------|-----------------------------------------------|
 * | Byte N+19  |                                               |
 * |   .        |          Timestamp seconds component          |
 * | Byte N+26  |                                               |
 * |------------|-----------------------------------------------|
 * | Byte N+27  |                                               |
 * |   .        |        Timestamp nanoseconds component        |
 * | Byte N+34  |                                               |
 * |____________|_______________________________________________|
 * | Byte N+35  |               Labels len MSB                  |
 * | Byte N+36  |               Labels len LSB                  |
 * |------------|-----------------------------------------------|<- Labels arr
 * | Byte N+37  |                Field len MSB                  |
 * | Byte N+38  |                Field len LSB                  |
 * |------------|-----------------------------------------------|
 * | Byte N+39  |                                               |
 * |   .        |                 Label name                    |
 * | Byte N+M   |                                               |
 * |------------|-----------------------------------------------|
 * | Byte K     |             Label Value len MSB               |
 * | Byte K+1   |             Label Value len LSB               |
 * |------------|-----------------------------------------------|
 * | Byte K+2   |                                               |
 * |   .        |                 Label Value                   |
 * | Byte K+N   |                                               |
 * |------------|-----------------------------------------------|
 *
 * The steps starting at [Array start] will be repeated until the expected
 * length of the packet is exhausted.
 */
static void unpack_tts_addpoints(uint8_t *buf, size_t len,
                                 struct tts_addpoints *a) {
    int64_t val = 0;
    // zero'ing tts_addpoints struct
    memset(a, 0x00, sizeof(*a));
    len -= unpack_integer(&buf, 'B', &val);
    a->ts_name_len = val;
    a->ts_name = malloc(a->ts_name_len + 1);
    len -= unpack_bytes(&buf, a->ts_name_len, a->ts_name);
    for (int i = 0; len > 0; ++i) {
        a->points = realloc(a->points, (i + 1) * sizeof(*a->points));
        len -= unpack_integer(&buf, 'B', &val);
        a->points[i].byte = val;
        len -= unpack_real(&buf, 'g', &a->points[i].value);
        // Unpack the seconds and nanoseconds component of the timestamp if
        // present
        if (a->points[i].bits.ts_sec_set == 1) {
            len -= unpack_integer(&buf, 'Q', &val);
            a->points[i].ts_sec = val;
        }
        if (a->points[i].bits.ts_nsec_set == 1) {
            len -= unpack_integer(&buf, 'Q', &val);
            a->points[i].ts_nsec = val;
        }
        len -= unpack_integer(&buf, 'H', &val);
        a->points[i].labels_len = val;
        a->points[i].labels =
            calloc(a->points[i].labels_len, sizeof(*a->points[i].labels));
        for (int j = 0; j < a->points[i].labels_len; ++j) {
            len -= unpack_integer(&buf, 'H', &val);
            a->points[i].labels[j].label_len = val;
            a->points[i].labels[j].label = malloc(val + 1);
            len -= unpack_bytes(&buf, a->points[i].labels[j].label_len,
                                a->points[i].labels[j].label);
            len -= unpack_integer(&buf, 'H', &val);
            a->points[i].labels[j].value_len = val;
            a->points[i].labels[j].value = malloc(val + 1);
            len -= unpack_bytes(&buf, a->points[i].labels[j].value_len,
                                a->points[i].labels[j].value);
        }
        ++a->points_len;
    }
}

/*
 * Unpack from binary to struct tts_query packet:
 *
 * |   Bit      |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 * |------------|-----------------------------------------------|
 * | Byte 5     |          Time series name len MSB             |
 * | Byte 6     |          Time series name len LSB             |
 * |------------|-----------------------------------------------|
 * | Byte 7     | avg |first| last|  gt |  lt |    reserved     |
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
    len -= unpack_integer(&buf, 'B', &val);
    q->ts_name_len = val;
    q->ts_name = malloc(q->ts_name_len + 1);
    len -= unpack_bytes(&buf, q->ts_name_len, q->ts_name);
    // We unpack the query header here, carrying the flags and filter to apply
    // to the requested query
    len -= unpack_integer(&buf, 'B', &val);
    q->byte = val;
    // Next fields are optional, we unpack them only after their flags
    if (q->bits.mean == 1)
        len -= unpack_integer(&buf, 'Q', (int64_t *) &q->mean_val);
    if (q->bits.major_of == 1)
        len -= unpack_integer(&buf, 'Q', (int64_t *) &q->major_of);
    if (q->bits.minor_of == 1)
        len -= unpack_integer(&buf, 'Q', (int64_t *) &q->minor_of);
    if (q->bits.filter == 1) {
        for (int i = 0; len > 0; ++i) {
            q->filters = realloc(q->filters, (i + 1) * sizeof(*q->filters));
            len -= unpack_integer(&buf, 'H', &val);
            q->filters[i].label_len = val;
            len -= unpack_bytes(&buf, val, q->filters[i].label);
            len -= unpack_integer(&buf, 'H', &val);
            q->filters[i].value_len = val;
            len -= unpack_bytes(&buf, val, q->filters[i].value);
        }
    }
}

/*
 * Unpack the binary buffer to a tts_query_response
 */
static void unpack_tts_query_response(uint8_t *buf, size_t len,
                                      struct tts_query_response *qa) {
    int64_t val = 0LL;
    memset(qa, 0x00, sizeof(*qa));
    for (size_t i = 0; len > 0; ++i) {
        qa->results = realloc(qa->results, (i + 1) * sizeof(*qa->results));
        len -= unpack(buf, "BQQgH", &qa->results[i].rc, &qa->results[i].ts_sec,
                      &qa->results[i].ts_nsec, &qa->results[i].value,
                      &qa->results[i].labels_len);
        buf += sizeof(uint8_t) + sizeof(uint16_t) +
            sizeof(long double) + sizeof(uint64_t) * 2;
        qa->results[i].labels =
            calloc(qa->results[i].labels_len, sizeof(*qa->results[i].labels));
        for (size_t j = 0; j < qa->results[i].labels_len; ++j) {
            len -= unpack_integer(&buf, 'H', &val);
            qa->results[i].labels[j].label_len = val;
            qa->results[i].labels[j].label =
                malloc(qa->results[i].labels[j].label_len + 1);
            len -= unpack_bytes(&buf, qa->results[i].labels[j].label_len,
                                qa->results[i].labels[j].label);
            len -= unpack_integer(&buf, 'H', &val);
            qa->results[i].labels[j].value_len = val;
            qa->results[i].labels[j].value =
                malloc(qa->results[i].labels[j].value_len + 1);
            len -= unpack_bytes(&buf, qa->results[i].labels[j].value_len,
                                qa->results[i].labels[j].value);
        }
        qa->len++;
    }
}

/*
 * Unpack a tts_packet, after reading the header opcode and the length of the
 * entire packet, calls the right unpack function based on the command type
 */
void unpack_tts_packet(uint8_t *buf, struct tts_packet *tts_p) {
    int64_t val = 0;
    tts_p->header.byte = *buf++;
    unpack_integer(&buf, 'I', &val);
    tts_p->len = val;
    if (tts_p->header.opcode == TTS_ACK)
        return;
    switch(tts_p->header.opcode) {
        case TTS_CREATE_TS:
            unpack_tts_create(buf, tts_p->len, &tts_p->create);
            break;
        case TTS_DELETE_TS:
            unpack_tts_delete(buf, tts_p->len, &tts_p->drop);
            break;
        case TTS_ADDPOINTS:
            unpack_tts_addpoints(buf, tts_p->len, &tts_p->addpoints);
            break;
        case TTS_QUERY:
            unpack_tts_query(buf, tts_p->len, &tts_p->query);
            break;
        case TTS_QUERY_RESPONSE:
            unpack_tts_query_response(buf, tts_p->len, &tts_p->query_r);
            break;
    }
}

/*
 * ========================
 *    PACKING FUNCTONS
 *========================
 */

static ssize_t pack_tts_create(const struct tts_create_ts *create,
                               uint8_t *buf) {
    return pack(buf, "BsI", create->ts_name_len,
                create->ts_name, create->retention);
}

static ssize_t pack_tts_delete(const struct tts_delete_ts *drop, uint8_t *buf) {
    return pack(buf, "Bs", drop->ts_name_len, drop->ts_name);
}

static ssize_t pack_tts_query(const struct tts_query *query, uint8_t *buf) {
    ssize_t len = pack(buf, "BsB", query->ts_name_len,
                       query->ts_name, query->byte);
    buf += len;
    if (query->bits.mean == 1)
        len += pack_integer(&buf, 'Q', query->mean_val);
    if (query->bits.major_of == 1)
        len += pack_integer(&buf, 'Q', query->major_of);
    if (query->bits.minor_of == 1)
        len += pack_integer(&buf, 'Q', query->minor_of);
    return len;
}

static ssize_t pack_tts_addpoints(const struct tts_addpoints *a, uint8_t *buf) {
    size_t len = pack(buf, "Bs", a->ts_name_len, a->ts_name);
    buf += len;
    for (int i = 0; i < a->points_len; ++i) {
        len += pack_integer(&buf, 'B', a->points[i].byte);
        len += pack_real(&buf, 'g', a->points[i].value);
        if (a->points[i].bits.ts_sec_set == 1)
            len += pack_integer(&buf, 'Q', a->points[i].ts_sec);
        if (a->points[i].bits.ts_nsec_set == 1)
            len += pack_integer(&buf, 'Q', a->points[i].ts_nsec);
        len += pack_integer(&buf, 'H', a->points[i].labels_len);
        for (int j = 0; j < a->points[i].labels_len; ++j) {
            len += pack(buf, "HsHs", a->points[i].labels[j].label_len,
                        a->points[i].labels[j].label,
                        a->points[i].labels[j].value_len,
                        a->points[i].labels[j].value);
            buf += sizeof(uint16_t) * 2 + a->points[i].labels[j].label_len +
                a->points[i].labels[j].value_len;
        }
    }
    return len;
}

ssize_t pack_tts_query_response(const struct tts_query_response *qr,
                                uint8_t *buf) {
    size_t len = 0LL;
    size_t packed = 0LL;
    for (uint64_t i = 0; i < qr->len; ++i) {
        packed = pack(buf, "BQQgH", qr->results[i].rc, qr->results[i].ts_sec,
                      qr->results[i].ts_nsec, qr->results[i].value,
                      qr->results[i].labels_len);
        buf += packed;
        len += packed;
        for (size_t j = 0; j < qr->results[i].labels_len; ++j) {
            packed = pack(buf, "HsHs",
                          qr->results[i].labels[j].label_len,
                          qr->results[i].labels[j].label,
                          qr->results[i].labels[j].value_len,
                          qr->results[i].labels[j].value);
            buf += packed;
            len += packed;
        }
    }
    return len;
}

ssize_t pack_tts_packet(const struct tts_packet *tts_p, uint8_t *buf) {
    int len_offset = sizeof(uint32_t);
    ssize_t len = pack_integer(&buf, 'B', tts_p->header.byte);
    ssize_t plen = 0;
    if (tts_p->header.opcode == TTS_ACK)
        goto encode_len;
    switch(tts_p->header.opcode) {
        case TTS_CREATE_TS:
            plen = pack_tts_create(&tts_p->create, buf + len_offset);
            break;
        case TTS_DELETE_TS:
            plen = pack_tts_delete(&tts_p->drop, buf + len_offset);
            break;
        case TTS_ADDPOINTS:
            plen = pack_tts_addpoints(&tts_p->addpoints, buf + len_offset);
            break;
        case TTS_QUERY:
            plen = pack_tts_query(&tts_p->query, buf + len_offset);
            break;
        case TTS_QUERY_RESPONSE:
            plen = pack_tts_query_response(&tts_p->query_r, buf + len_offset);
            break;
    }
encode_len:
    len += plen;
    len += pack_integer(&buf, 'I', plen);
    return len;
}

void tts_packet_destroy(struct tts_packet *packet) {
    switch (packet->header.opcode) {
        case TTS_CREATE_TS:
            free(packet->create.ts_name);
            break;
        case TTS_DELETE_TS:
            free(packet->drop.ts_name);
            break;
        case TTS_ADDPOINTS:
            free(packet->addpoints.ts_name);
            for (int i = 0; i < packet->addpoints.points_len; ++i)
                free(packet->addpoints.points[i].labels);
            free(packet->addpoints.points);
            break;
        case TTS_QUERY:
            free(packet->query.ts_name);
            break;
    }
}
