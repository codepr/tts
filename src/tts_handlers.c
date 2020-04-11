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

#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include "tts_protocol.h"
#include "tts_handlers.h"

static int handle_tts_create(struct tts_payload *payload) {
    ev_buf *buf = payload->buf;
    struct tts_packet *packet = &payload->packet;
    struct tts_timeseries *ts = malloc(sizeof(*ts));
    TTS_TIMESERIES_INIT(ts, packet->create.ts_name, packet->create.retention);
    HASH_ADD_STR(payload->tts_db->timeseries, name, ts);
    struct tts_packet response = {0};
    TTS_SET_RESPONSE_HEADER(&response, TTS_ACK, TTS_OK);
    buf->size = pack_tts_packet(&response, (uint8_t *) buf->buf);
    return TTS_OK;
}

static int handle_tts_delete(struct tts_payload *payload) {
    ev_buf *buf = payload->buf;
    struct tts_packet *packet = &payload->packet;
    struct tts_timeseries *ts = NULL;
    char *key = (char *) packet->drop.ts_name;
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    int rc = TTS_OK;
    if (!ts)
        rc = TTS_ENOTS;
    else
        HASH_DEL(payload->tts_db->timeseries, ts);
    struct tts_packet response = {0};
    TTS_SET_RESPONSE_HEADER(&response, TTS_ACK, rc);
    buf->size = pack_tts_packet(&response, (uint8_t *) buf->buf);
    return TTS_OK;
}

static int handle_tts_addpoints(struct tts_payload *payload) {
    ev_buf *buf = payload->buf;
    struct tts_addpoints *pa = &payload->packet.addpoints;
    struct tts_timeseries *ts = NULL;
    char *key = (char *) pa->ts_name;
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    struct tts_packet response = {0};
    int rc = TTS_OK;
    if (!ts) {
        rc = TTS_ENOTS;
    } else {
        struct timespec tv;
        struct tts_tag *dummy = NULL, *sub = NULL;
        clock_gettime(CLOCK_REALTIME, &tv);
        for (int i = 0; i < pa->points_len; i++) {
            if (pa->points[i].bits.ts_sec_set == 0)
                pa->points[i].ts_sec = tv.tv_sec;
            if (pa->points[i].bits.ts_nsec_set == 0)
                pa->points[i].ts_nsec = tv.tv_nsec;
            unsigned long long timestamp =
                pa->points[i].ts_sec * 1e9 +
                pa->points[i].ts_nsec;
            TTS_VECTOR_APPEND(ts->timestamps, timestamp);
            struct tts_record *record = malloc(sizeof(*record));
            record->index = TTS_VECTOR_SIZE(ts->timestamps);
            record->value = pa->points[i].value;
            record->labels_nr = pa->points[i].labels_len;
            record->labels = calloc(pa->points[i].labels_len,
                                    sizeof(*record->labels));
            for (int j = 0; j < pa->points[i].labels_len; ++j) {
                HASH_FIND_STR(ts->tags, (char *)
                              pa->points[i].labels[j].label, dummy);
                if (dummy) {
                    HASH_FIND_STR(dummy->tag, (char *)
                                  pa->points[i].labels[j].value, sub);
                    if (!sub) {
                        struct tts_tag *tag = malloc(sizeof(*tag));
                        tag->tag_name = (char *) pa->points[i].labels[j].value;
                        TTS_VECTOR_NEW(tag->column);
                        HASH_ADD_STR(dummy->tag, tag_name, tag);
                        sub = tag;
                    }
                    TTS_VECTOR_APPEND(sub->column, record);
                } else {
                    struct tts_tag *tag = malloc(sizeof(*tag));
                    tag->tag = NULL;
                    tag->tag_name = (char *) pa->points[i].labels[j].label;
                    TTS_VECTOR_NEW(tag->column);
                    HASH_ADD_STR(ts->tags, tag_name, tag);
                    struct tts_tag *sub_tag = malloc(sizeof(*sub_tag));
                    sub_tag->tag_name = (char *) pa->points[i].labels[j].value;
                    TTS_VECTOR_NEW(sub_tag->column);
                    TTS_VECTOR_APPEND(sub_tag->column, record);
                    HASH_ADD_STR(tag->tag, tag_name, sub_tag);
                }
                record->labels[j].field =
                    (char *) pa->points[i].labels[j].label;
                record->labels[j].value = pa->points[i].labels[j].value;
            }
            TTS_VECTOR_APPEND(ts->columns, record);
        }
    }
    TTS_SET_RESPONSE_HEADER(&response, TTS_ACK, rc);
    buf->size = pack_tts_packet(&response, (uint8_t *) buf->buf);
    return TTS_OK;
}

static void handle_tts_query_single(const struct tts_timeseries *ts,
                                    struct tts_query_response *q,
                                    size_t r_idx, size_t t_idx) {
    unsigned long long t = TTS_VECTOR_AT(ts->timestamps, t_idx);
    struct tts_record *record = TTS_VECTOR_AT(ts->columns, t_idx);
    q->results[r_idx].rc = TTS_OK;
    q->results[r_idx].ts_sec = t / (unsigned long long) 1e9;
    q->results[r_idx].ts_nsec = t % (unsigned long long) 1e9;
    q->results[r_idx].value = record->value;
    q->results[r_idx].labels_len = record->labels_nr;
    q->results[r_idx].labels =
        calloc(record->labels_nr, sizeof(*q->results[r_idx].labels));
    for (size_t j = 0; j < record->labels_nr; ++j) {
        q->results[r_idx].labels[j].label_len = strlen(record->labels[j].field);
        q->results[r_idx].labels[j].label = (uint8_t *) record->labels[j].field;
        q->results[r_idx].labels[j].value_len = strlen(record->labels[j].value);
        q->results[r_idx].labels[j].value = record->labels[j].value;
    }
}

static void handle_tts_query_all(const struct tts_timeseries *ts,
                                 struct tts_packet *p,
                                 ev_buf *buf) {
    struct tts_query_response *q = &p->query_r;
    size_t colsize = TTS_VECTOR_SIZE(ts->timestamps);
    q->results = calloc(colsize, sizeof(*q->results));
    q->len = colsize;
    for (size_t i = 0; i < colsize; ++i)
        handle_tts_query_single(ts, q, i, i);
    buf->size = pack_tts_packet(p, (uint8_t *) buf->buf);
    for (size_t i = 0; i < colsize; ++i)
        free(q->results[i].labels);
    free(q->results);
}

static void get_range_indexes(const struct tts_timeseries *ts,
                              size_t minor_of, size_t major_of,
                              size_t *lo_idx, size_t *hi_idx) {
    TTS_VECTOR_BINSEARCH(ts->timestamps, major_of, lo_idx);
    TTS_VECTOR_BINSEARCH(ts->timestamps, minor_of, hi_idx);
    // Update range to include neighbour values
    for (size_t i = *lo_idx - 1; i > 0 &&
         TTS_VECTOR_AT(ts->timestamps, i) >= major_of; --i) {
        --*lo_idx;
    }
    for (size_t i = *hi_idx + 1; i < TTS_VECTOR_SIZE(ts->timestamps) &&
         TTS_VECTOR_AT(ts->timestamps, i) <= minor_of; ++i) {
        ++*hi_idx;
    }
}

static void handle_tts_query_range(const struct tts_timeseries *ts,
                                   struct tts_packet *p,
                                   size_t minor_of,
                                   size_t major_of,
                                   ev_buf *buf) {
    size_t lo_idx = 0UL, hi_idx = 0UL;
    struct tts_query_response *q = &p->query_r;
    get_range_indexes(ts, minor_of, major_of, &lo_idx, &hi_idx);
    unsigned long long range = hi_idx - lo_idx + 1;
    q->results = calloc(range, sizeof(*q->results));
    q->len = range;
    for (size_t i = 0; i < range; ++i)
        handle_tts_query_single(ts, q, i, i + lo_idx);
    buf->size = pack_tts_packet(p, (uint8_t *) buf->buf);
    for (size_t i = 0; i < range; ++i)
        free(q->results[i].labels);
    free(q->results);
}

static void handle_tts_query_one(const struct tts_timeseries *ts,
                                 struct tts_packet *p,
                                 size_t idx,
                                 ev_buf *buf) {
    struct tts_query_response *q = &p->query_r;
    size_t colsize = 1;
    q->results = calloc(colsize, sizeof(*q->results));
    q->len = colsize;
    handle_tts_query_single(ts, q, 0, idx);
    buf->size = pack_tts_packet(p, (uint8_t *) buf->buf);
    for (size_t i = 0; i < colsize; ++i)
        free(q->results[i].labels);
    free(q->results);
}

static void handle_tts_query_mean(const struct tts_timeseries *ts,
                                  struct tts_packet *p,
                                  size_t lo, size_t hi,
                                  unsigned long long window,
                                  ev_buf *buf) {
    struct tts_query_response *q = &p->query_r;
    long double avg = 0.0;
    unsigned long long t = 0ULL, step = 0LL;
    struct tts_record *record = NULL;
    q->results = NULL;
    size_t i = lo, j = 0, k = 0;
    while (i < hi) {
        q->results = realloc(q->results, (k + 1) * sizeof(*q->results));
        j = 0;
        step = TTS_VECTOR_AT(ts->timestamps, i) + window * 1e6;
        avg = 0;
        for (;TTS_VECTOR_AT(ts->timestamps, i) <= step; ++i) {
            t = TTS_VECTOR_AT(ts->timestamps, i);
            record = TTS_VECTOR_AT(ts->columns, i);
            avg += record->value;
            ++j;
        }
        avg /= j;
        q->results[k].labels_len = 0;
        q->results[k].rc = TTS_OK;
        q->results[k].ts_sec = t / (unsigned long long) 1e9;
        q->results[k].ts_nsec = t % (unsigned long long) 1e9;
        q->results[k].value = avg;
        ++k;
        ++q->len;
    }
    buf->size = pack_tts_packet(p, (uint8_t *) buf->buf);
    free(q->results);
}

//static void handle_tts_query_meanp(const struct tts_timeseries *ts,
//                                   struct tts_packet *p,
//                                   size_t lo, size_t hi,
//                                   unsigned long long start,
//                                   unsigned long long window,
//                                   ev_buf *buf) {
//    struct tts_query_ack *q = &p->query_ack;
//    long double avg = 0.0;
//    unsigned long long step = 0LL, base = 0LL;
//    struct tts_record *record = NULL;
//    q->results = NULL;
//    size_t i = lo, j = 0, k = 0;
//    start *= 1e6; window *= 1e6;
//    while (TTS_VECTOR_AT(ts->timestamps, lo) > start) {
//        if (start + window > TTS_VECTOR_AT(ts->timestamps, lo))
//            break;
//        start += window;
//    }
//    base = start;
//    while (i < hi) {
//        q->results = realloc(q->results, (k + 1) * sizeof(*q->results));
//        j = 0;
//        step = base + window;
//        avg = 0;
//        for (;TTS_VECTOR_AT(ts->timestamps, i) <= step; ++i) {
//            record = TTS_VECTOR_AT(ts->columns, i);
//            avg += record->value;
//            ++j;
//        }
//        avg /= j;
//        q->results[k].labels_len = 0;
//        q->results[k].rc = TTS_OK;
//        q->results[k].ts_sec = step / (unsigned long long) 1e9;
//        q->results[k].ts_nsec = step % (unsigned long long) 1e9;
//        q->results[k].value = avg;
//        ++k;
//        ++q->len;
//    }
//    buf->size = pack_tts_packet(p, (uint8_t *) buf->buf);
//    free(q->results);
//}

static int handle_tts_query(struct tts_payload *payload) {
    ev_buf *buf = payload->buf;
    struct tts_packet *packet = &payload->packet;
    struct tts_timeseries *ts = NULL;
    char *key = (char *) packet->query.ts_name;
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    int rc = TTS_OK;
    struct tts_packet response = {0};
    if (!ts) {
        rc = TTS_ENOTS;
        TTS_SET_RESPONSE_HEADER(&response, TTS_ACK, rc);
        buf->size = pack_tts_packet(&response, (uint8_t *) buf->buf);
    } else {
        TTS_SET_RESPONSE_HEADER(&response, TTS_QUERY_RESPONSE, rc);
        if (packet->query.byte == TTS_QUERY_ALL_TIMESERIES ||
            packet->query.byte == TTS_QUERY_ALL_TIMESERIES_AVG) {
            if (packet->query.bits.mean == 0) {
                handle_tts_query_all(ts, &response, buf);
            } else {
                size_t hi = TTS_VECTOR_SIZE(ts->timestamps);
                handle_tts_query_mean(ts, &response, 0, hi,
                                      packet->query.mean_val, buf);
            }
        } else {
            size_t ts_size = TTS_VECTOR_SIZE(ts->timestamps) - 1;
            unsigned long long major_of = TTS_VECTOR_AT(ts->timestamps, 0);
            unsigned long long minor_of = TTS_VECTOR_AT(ts->timestamps, ts_size);
            if (packet->query.bits.first == 1) {
                handle_tts_query_one(ts, &response, 0, buf);
            } else if (packet->query.bits.last == 1) {
                size_t idx = TTS_VECTOR_SIZE(ts->timestamps) - 1;
                handle_tts_query_one(ts, &response, idx, buf);
            } else {
                if (packet->query.bits.major_of == 1)
                    major_of = packet->query.major_of;
                if (packet->query.bits.minor_of == 1)
                    minor_of = packet->query.minor_of;
                if (packet->query.bits.mean == 0) {
                    handle_tts_query_range(ts, &response, minor_of, major_of, buf);
                } else {
                    size_t lo_idx = 0UL, hi_idx = 0UL;
                    get_range_indexes(ts, minor_of, major_of, &lo_idx, &hi_idx);
                    //handle_tts_query_meanp(ts, &response, lo_idx, hi_idx,
                    //                       major_of, packet->query.mean_val, buf);
                    handle_tts_query_mean(ts, &response, lo_idx, hi_idx,
                                          packet->query.mean_val, buf);
                }
            }
        }
    }
    return TTS_OK;
}

int tts_handle_packet(struct tts_payload *payload) {
    int rc = 0;
    switch (payload->packet.header.opcode) {
        case TTS_CREATE_TS:
            rc = handle_tts_create(payload);
            break;
        case TTS_DELETE_TS:
            rc = handle_tts_delete(payload);
            break;
        case TTS_ADDPOINTS:
            rc = handle_tts_addpoints(payload);
            break;
        case TTS_QUERY:
            rc = handle_tts_query(payload);
            break;
    }
    return rc;
}
