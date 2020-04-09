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
    TTS_TIMESERIES_INIT(ts, packet->create.ts_name);
    //for (int i = 0; i < packet->create.fields_len; i++)
    //    TTS_VECTOR_APPEND(ts->fields, (char *) packet->create.fields[i].field);
    // add to db hashmap
    HASH_ADD_STR(payload->tts_db->timeseries, name, ts);
    struct tts_packet response = {
        .header.byte = TTS_ACK,
        .ack = (struct tts_ack) {
            .rc = TTS_OK
        }
    };
    buf->size = pack_tts_packet(&response, (uint8_t *) buf->buf);
    return TTS_OK;
}

static int handle_tts_delete(struct tts_payload *payload) {
    ev_buf *buf = payload->buf;
    struct tts_packet *packet = &payload->packet;
    struct tts_timeseries *ts = NULL;
    char *key = (char *) packet->drop.ts_name;
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    struct tts_packet response = {
        .header.byte = TTS_ACK,
        .ack = (struct tts_ack) {
            .rc = TTS_OK
        }
    };
    if (!ts)
        response.ack.rc = TTS_OK;
    else
        HASH_DEL(payload->tts_db->timeseries, ts);
    buf->size = pack_tts_packet(&response, (uint8_t *) buf->buf);
    return TTS_OK;
}

static int handle_tts_addpoints(struct tts_payload *payload) {
    ev_buf *buf = payload->buf;
    struct tts_addpoints *pa = &payload->packet.addpoints;
    struct tts_timeseries *ts = NULL;
    char *key = (char *) pa->ts_name;
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    struct tts_packet response = {
        .header.byte = TTS_ACK,
        .ack = (struct tts_ack) {
            .rc = TTS_OK
        }
    };
    if (!ts) {
        response.ack.rc = TTS_NOK;
    } else {
        struct timespec tv;
        clock_gettime(CLOCK_REALTIME, &tv);
        for (int i = 0; i < pa->points_len; i++) {
            if (pa->points[i].ts_flags.bits.ts_sec_set == 0)
                pa->points[i].ts_sec = tv.tv_sec;
            if (pa->points[i].ts_flags.bits.ts_nsec_set == 0)
                pa->points[i].ts_nsec = tv.tv_nsec;
            unsigned long long timestamp =
                pa->points[i].ts_sec * 1e9 +
                pa->points[i].ts_nsec;
            TTS_VECTOR_APPEND(ts->timestamps, timestamp);
            struct tts_record *record = malloc(sizeof(*record));
            record->value = pa->points[i].value;
            record->labels_nr = pa->points[i].labels_len;
            record->labels = calloc(pa->points[i].labels_len,
                                    sizeof(*record->labels));
            for (int j = 0; j < pa->points[i].labels_len; ++j) {
                record->labels[j].field =
                    (char *) pa->points[i].labels[j].label;
                record->labels[j].value = pa->points[i].labels[j].value;
            }
            TTS_VECTOR_APPEND(ts->columns, record);
        }
    }
    buf->size = pack_tts_packet(&response, (uint8_t *) buf->buf);
    return TTS_OK;
}

static void handle_tts_query_single(const struct tts_timeseries *ts,
                                    struct tts_query_ack *q,
                                    size_t r_idx, size_t t_idx) {
    unsigned long long t = TTS_VECTOR_AT(ts->timestamps, t_idx);
    struct tts_record *record = TTS_VECTOR_AT(ts->columns, t_idx);
    q->results[r_idx].rc = TTS_OK;
    q->results[r_idx].ts_sec = t / (unsigned long long) 1e9;
    q->results[r_idx].ts_nsec = t % (unsigned long long) 1e9;
    q->results[r_idx].value = record->value;
    q->results[r_idx].res_len = record->labels_nr;
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
    struct tts_query_ack *q = &p->query_ack;
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
    struct tts_query_ack *q = &p->query_ack;
    get_range_indexes(ts, minor_of, major_of, &lo_idx, &hi_idx);
    //TTS_VECTOR_BINSEARCH(ts->timestamps, major_of, &lo_idx);
    //TTS_VECTOR_BINSEARCH(ts->timestamps, minor_of, &hi_idx);
    //// Update range to include neighbour values
    //for (size_t i = lo_idx - 1; i > 0 &&
    //     TTS_VECTOR_AT(ts->timestamps, i) >= major_of;
    //     --i) {
    //    --lo_idx;
    //}
    //for (size_t i = hi_idx + 1; i < TTS_VECTOR_SIZE(ts->timestamps) &&
    //     TTS_VECTOR_AT(ts->timestamps, i) <= minor_of;
    //     ++i) {
    //    ++hi_idx;
    //}
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
    struct tts_query_ack *q = &p->query_ack;
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
    struct tts_query_ack *q = &p->query_ack;
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
        q->results[k].res_len = 0;
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

static int handle_tts_query(struct tts_payload *payload) {
    ev_buf *buf = payload->buf;
    struct tts_packet *packet = &payload->packet;
    struct tts_timeseries *ts = NULL;
    char *key = (char *) packet->query.ts_name;
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    struct tts_packet response = {
        .header.byte = TTS_ACK,
        .ack = (struct tts_ack) {
            .rc = TTS_OK
        }
    };
    if (!ts) {
        response.ack.rc = TTS_NOK;
        buf->size = pack_tts_packet(&response, (uint8_t *) buf->buf);
    } else {
        response.header.byte = TTS_QUERY_RESPONSE;
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
                    handle_tts_query_mean(ts, &response, lo_idx, hi_idx,
                                          packet->query.mean_val, buf);
                }
            }
            //if (packet->query.bits.mean == 1) {
            //    if (packet->query.bits.mean_field == 1) {
            //        size_t j = 0, sum = 0, count = 0;
            //        struct tts_query_ack qq;
            //        memset(&qq, 0x00, sizeof(qq));
            //        unsigned long long step = packet->query.mean_val * 1e6;
            //        unsigned long long next = qa->results[0].ts_sec * 1e9 +
            //                                  qa->results[0].ts_nsec + step;
            //        size_t i = 0;
            //        while (i < qa->len) {
            //            qq.results = realloc(qq.results,
            //                                 (j+1) * sizeof(*qq.results));
            //            qq.results[j].res_len = 1;
            //            qq.results[j].points =
            //                calloc(1, sizeof(*qq.results[j].points));
            //            qq.results[j].points[0].field_len = packet->query.mean_field_len;
            //            qq.results[j].points[0].field = packet->query.mean_field;
            //            sum = 0;
            //            count = 0;
            //            while (qa->results[i].ts_sec * 1e9 +
            //                   qa->results[i].ts_nsec < next) {
            //                for (size_t k = 0; k < qa->results[i].res_len; ++k) {
            //                    if (strcmp((char *) qa->results[i].points[k].field,
            //                               (char *) packet->query.mean_field) == 0) {
            //                        sum += atoi((char *) qa->results[i].points[k].value);
            //                        count++;
            //                    }
            //                }
            //                ++i;
            //            }
            //            sum /= count == 0 ? 1 : count;
            //            qq.results[j].points[0].value = malloc(21);
            //            snprintf((char *) qq.results[j].points[0].value, 21, "%lu", sum);
            //            qq.results[j].points[0].value_len =
            //                strlen((char *) qq.results[j].points[0].value);
            //            ++j;
            //            next += step;
            //        }
            //        qa = &qq;
            //    }
            //}
        }
    }
    return TTS_OK;
}

int tts_handle_packet(struct tts_payload *payload) {
    int rc = 0;
    switch (payload->packet.header.byte) {
        case TTS_CREATE:
            rc = handle_tts_create(payload);
            break;
        case TTS_DELETE:
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
