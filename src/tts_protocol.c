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
static void unpack_tts_create(uint8_t *buf, size_t len, struct tts_create *c) {
    int64_t val = 0;
    // zero'ing tts_create struct
    memset(c, 0x00, sizeof(*c));
    len -= unpack_integer(&buf, 'B', &val);
    c->ts_name_len = val;
    c->ts_name = malloc(c->ts_name_len + 1);
    len -=  unpack_bytes(&buf, c->ts_name_len, c->ts_name);
    //for (int i = 0; len > 0; ++i) {
    //    /*
    //     * Probably not the best way to handle it but we have to make room for
    //     * additional incoming tuples
    //     */
    //    c->fields = realloc(c->fields, (i + 1) * sizeof(*c->fields));
    //    len -= unpack_integer(&buf, 'H', &val);
    //    c->fields[i].field_len = val;
    //    c->fields[i].field = malloc(c->fields[i].field_len + 1);
    //    len -= unpack_bytes(&buf, c->fields[i].field_len, c->fields[i].field);
    //    ++c->fields_len;
    //}
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
    len -= unpack_integer(&buf, 'B', &val);
    a->ts_name_len = val;
    a->ts_name = malloc(a->ts_name_len + 1);
    len -= unpack_bytes(&buf, a->ts_name_len, a->ts_name);
    for (int i = 0; len > 0; ++i) {
        a->points = realloc(a->points, (i + 1) * sizeof(*a->points));
        len -= unpack_integer(&buf, 'B', &val);
        a->points[i].ts_flags.byte = val;
        len -= unpack_real(&buf, 'g', &a->points[i].value);
        // Unpack the seconds and nanoseconds component of the timestamp if
        // present
        if (a->points[i].ts_flags.bits.ts_sec_set == 1) {
            len -= unpack_integer(&buf, 'Q', &val);
            a->points[i].ts_sec = val;
        }
        if (a->points[i].ts_flags.bits.ts_nsec_set == 1) {
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
    len -= unpack_integer(&buf, 'B', &val);
    q->byte = val;
    len -= unpack_integer(&buf, 'B', &val);
    q->ts_name_len = val;
    q->ts_name = malloc(q->ts_name_len + 1);
    len -= unpack_bytes(&buf, q->ts_name_len, q->ts_name);
    if (q->bits.mean == 1) {
        len -= unpack_integer(&buf, 'Q', (int64_t *) &q->mean_val);
        if (q->bits.mean_field == 1) {
            len -= unpack_integer(&buf, 'H', (int64_t *) &q->mean_field_len);
            q->mean_field = malloc(q->mean_field_len + 1);
            len -= unpack_bytes(&buf, q->mean_field_len, q->mean_field);
        }
    }
    if (q->bits.major_of == 1)
        len -= unpack_integer(&buf, 'Q', (int64_t *) &q->major_of);
    if (q->bits.minor_of == 1)
        len -= unpack_integer(&buf, 'Q', (int64_t *) &q->minor_of);
}

static void unpack_tts_query_response(uint8_t *buf, size_t len,
                                      struct tts_query_ack *qa) {
    int64_t val = 0LL;
    memset(qa, 0x00, sizeof(*qa));
    for (size_t i = 0; len > 0; ++i) {
        qa->results = realloc(qa->results, (i + 1) * sizeof(*qa->results));
        len -= unpack(buf, "BQQgH", &qa->results[i].rc, &qa->results[i].ts_sec,
                      &qa->results[i].ts_nsec, &qa->results[i].value,
                      &qa->results[i].res_len);
        buf += sizeof(uint8_t) + sizeof(uint16_t) +
            sizeof(long double) + sizeof(uint64_t) * 2;
        qa->results[i].labels =
            calloc(qa->results[i].res_len, sizeof(*qa->results[i].labels));
        for (size_t j = 0; j < qa->results[i].res_len; ++j) {
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

static void unpack_tts_ack(uint8_t *buf, size_t len, struct tts_ack *a) {
    (void) len;
    memset(a, 0x00, sizeof(*a));
    unpack_integer(&buf, 'B', (int64_t *) &a->rc);
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
        case TTS_ACK:
            unpack_tts_ack(buf, len, &tts_p->ack);
            break;
        case TTS_QUERY_RESPONSE:
            unpack_tts_query_response(buf, len, &tts_p->query_ack);
            break;
    }
}

/*
 * ========================
 *    PACKING FUNCTONS
 *========================
 */

static ssize_t pack_tts_create(const struct tts_create *create, uint8_t *buf) {
    ssize_t len = 0LL;
    uint8_t *ptr = buf;
    buf += sizeof(uint32_t);
    len += pack(buf, "Bs", create->ts_name_len, create->ts_name);
    buf += sizeof(uint8_t) + create->ts_name_len;
    //for (int i = 0; i < create->fields_len; ++i) {
    //    len += pack(buf, "Hs", create->fields[i].field_len,
    //                create->fields[i].field);
    //    buf += sizeof(uint16_t) + create->fields[i].field_len;
    //}
    pack_integer(&ptr, 'I', len);
    len += sizeof(uint8_t) + sizeof(uint32_t);
    return len;
}

static ssize_t pack_tts_delete(const struct tts_delete *drop, uint8_t *buf) {
    ssize_t len = sizeof(uint8_t) + drop->ts_name_len;
    len += pack(buf, "IBs", len, drop->ts_name_len, drop->ts_name);
    return len;
}

static ssize_t pack_tts_query(const struct tts_query *query, uint8_t *buf) {
    uint8_t *ptr = buf;
    buf += sizeof(uint32_t);
    ssize_t len = pack(buf, "BBs", query->byte,
                       query->ts_name_len, query->ts_name);
    buf += len;
    if (query->bits.mean == 1) {
        len += pack_integer(&buf, 'Q', query->mean_val);
        if (query->bits.mean_field == 1) {
            len += pack(buf, "Hs", query->mean_field_len, query->mean_field);
            buf += sizeof(uint16_t) + query->mean_field_len;
        }
    }
    if (query->bits.major_of == 1)
        len += pack_integer(&buf, 'Q', query->major_of);
    if (query->bits.minor_of == 1)
        len += pack_integer(&buf, 'Q', query->minor_of);
    len += pack_integer(&ptr, 'I', len);
    return len;
}

static ssize_t pack_tts_addpoints(const struct tts_addpoints *a, uint8_t *buf) {
    size_t len = 0;
    uint8_t *ptr = (uint8_t *) buf;
    buf += sizeof(uint32_t);
    len += pack(buf, "Bs", a->ts_name_len, a->ts_name);
    buf += len;
    for (int i = 0; i < a->points_len; ++i) {
        len += pack_integer(&buf, 'B', a->points[i].ts_flags.byte);
        len += pack_real(&buf, 'g', a->points[i].value);
        if (a->points[i].ts_flags.bits.ts_sec_set == 1)
            len += pack_integer(&buf, 'Q', a->points[i].ts_sec);
        if (a->points[i].ts_flags.bits.ts_nsec_set == 1)
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
    len += pack_integer(&ptr, 'I', len);
    return len;
}

/*
 * Simple helper to set a bytes buffer to an ACK response with a defined RC
 */
static ssize_t pack_tts_ack(const struct tts_ack *ack, uint8_t *buf) {
    return pack(buf, "IB", 1, ack->rc);
}

ssize_t pack_tts_query_response(const struct tts_query_ack *qa, uint8_t *buf) {
    size_t len = 0LL;
    size_t packed = 0LL;
    uint8_t *ptr = buf + sizeof(uint32_t);
    for (uint64_t i = 0; i < qa->len; ++i) {
        packed = pack(ptr, "BQQgH", qa->results[i].rc, qa->results[i].ts_sec,
                      qa->results[i].ts_nsec, qa->results[i].value,
                      qa->results[i].res_len);
        ptr += packed;
        len += packed;
        for (size_t j = 0; j < qa->results[i].res_len; ++j) {
            packed = pack(ptr, "HsHs",
                          qa->results[i].labels[j].label_len,
                          qa->results[i].labels[j].label,
                          qa->results[i].labels[j].value_len,
                          qa->results[i].labels[j].value);
            len += packed;
            ptr += packed;
        }
    }
    len += pack_integer(&buf, 'I', (uint32_t) len);
    return len;
}

ssize_t pack_tts_packet(const struct tts_packet *tts_p, uint8_t *buf) {
    ssize_t len = pack_integer(&buf, 'B', tts_p->header.byte);
    switch(tts_p->header.byte) {
        case TTS_CREATE:
            len += pack_tts_create(&tts_p->create, buf);
            break;
        case TTS_DELETE:
            len += pack_tts_delete(&tts_p->drop, buf);
            break;
        case TTS_ADDPOINTS:
            len += pack_tts_addpoints(&tts_p->addpoints, buf);
            break;
        case TTS_QUERY:
            len += pack_tts_query(&tts_p->query, buf);
            break;
        case TTS_ACK:
            len += pack_tts_ack(&tts_p->ack, buf);
            break;
        case TTS_QUERY_RESPONSE:
            len += pack_tts_query_response(&tts_p->query_ack, buf);
            break;
    }
    return len;
}

void tts_packet_destroy(struct tts_packet *packet) {
    switch (packet->header.byte) {
        case TTS_CREATE:
            free(packet->create.ts_name);
            //for (int i = 0; i < packet->create.; ++i)
            //    free(packet->create.fields[i].field);
            //free(packet->create.fields);
            break;
        case TTS_DELETE:
            free(packet->drop.ts_name);
            break;
        case TTS_ADDPOINTS:
            free(packet->addpoints.ts_name);
            for (int i = 0; i < packet->addpoints.points_len; ++i) {
                free(packet->addpoints.points[i].labels);
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
