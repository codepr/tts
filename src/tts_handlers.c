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
#include "tts_log.h"
#include "tts_protocol.h"
#include "tts_handlers.h"

static int handle_tts_create(struct tts_payload *payload) {
    ev_buf *buf = payload->buf;
    int rc = TTS_OK;
    struct tts_create_ts *c = &payload->packet.create;
    struct tts_packet response = {0};
    struct tts_timeseries *ts;
    char *key = (char *) c->ts_name;
    /*
     * First check that the timeseries doesn't exists already, returning a
     * TTS_ENOTS status code in case
     */
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    if (ts) {
        rc = TTS_ENOTS;
        log_debug("Timeseries \"%s\" exists already", c->ts_name);
    } else {
        /* If it does not exist we just create it and add to the global DB */
        ts = malloc(sizeof(*ts));
        TTS_TIMESERIES_INIT(ts, c->ts_name, c->retention);
        HASH_ADD_STR(payload->tts_db->timeseries, name, ts);
        log_debug("Created new timeseries \"%s\" (r=%u)", ts->name, ts->retention);
    }
    /* Set up the response to the client */
    TTS_SET_RESPONSE_HEADER(&response, TTS_ACK, rc);
    buf->size = pack_tts_packet(&response, (uint8_t *) buf->buf);
    return TTS_OK;
}

static int handle_tts_delete(struct tts_payload *payload) {
    ev_buf *buf = payload->buf;
    struct tts_packet *packet = &payload->packet;
    struct tts_packet response = {0};
    struct tts_timeseries *ts = NULL;
    char *key = (char *) packet->drop.ts_name;
    /*
     * First check that the timeseries doesn't exists already, returning a
     * TTS_ENOTS status code in case
     */
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    int rc = TTS_OK;
    if (!ts) {
        log_debug("Timeseries \"%s\" not found", packet->drop.ts_name);
        rc = TTS_ENOTS;
    } else {
        /* Just remove the entry from the global timeseries DB and destroy it */
        log_debug("Deleted \"%s\" timeseries", ts->name);
        HASH_DEL(payload->tts_db->timeseries, ts);
        TTS_TIMESERIES_DESTROY(ts);
    }
    /* Set up the response to the client */
    TTS_SET_RESPONSE_HEADER(&response, TTS_ACK, rc);
    buf->size = pack_tts_packet(&response, (uint8_t *) buf->buf);
    return TTS_OK;
}

static int handle_tts_addpoints(struct tts_payload *payload) {
    ev_buf *buf = payload->buf;
    struct tts_addpoints *pa = &payload->packet.addpoints;
    struct tts_timeseries *ts = NULL;
    char *key = (char *) pa->ts_name;
    struct tts_packet response = {0};
    unsigned long long timestamp = 0ULL;
    int rc = TTS_OK;
    /*
     * Check if the target timeseries already exists, if it doesn't we want to
     * create it in place and track it by storing into the global timeseries
     * DB
     */
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    if (!ts) {
        ts = malloc(sizeof(*ts));
        TTS_TIMESERIES_INIT(ts, key, 0);
        HASH_ADD_STR(payload->tts_db->timeseries, name, ts);
        log_debug("Timeseries \"%s\" not found, created now (r=0)", ts->name);
    }
    struct timespec tv;
    struct tts_tag *dummy = NULL, *sub = NULL;
    clock_gettime(CLOCK_REALTIME, &tv);
    /*
     * As it's possible to insert multiple points with the same timestamp, we
     * iterate point by point appending each one to the timestamps vector and
     * fill the columns with values and/or labels if present
     */
    for (int i = 0; i < pa->points_len; i++) {
        if (pa->points[i].bits.ts_sec_set == 0)
            pa->points[i].ts_sec = tv.tv_sec;
        if (pa->points[i].bits.ts_nsec_set == 0)
            pa->points[i].ts_nsec = tv.tv_nsec;
        timestamp = pa->points[i].ts_sec * 1e9 + pa->points[i].ts_nsec;
        TTS_VECTOR_APPEND(ts->timestamps, timestamp);
        struct tts_record *record = malloc(sizeof(*record));
        record->index = TTS_VECTOR_SIZE(ts->timestamps);
        record->value = pa->points[i].value;
        record->labels_nr = pa->points[i].labels_len;
        record->labels = calloc(pa->points[i].labels_len,
                                sizeof(*record->labels));
        /*
         * Labels can be in arbitrary number, to simplify they're treated as
         * strings which will act as keys on a multilevel hashmap.
         * For each label we want to first check if it exists already on the
         * timeseries `tags` pointer map, and if it doesn't we create it in
         * place and fill it with the value key on the next level.
         *
         * The hierarchy is simply a map of maps:
         *
         * TS:tags : {
         *      label_name: {
         *          label_value: vector[tts_record],
         *               .
         *               .
         *          label_valueN: vector[tts_record]
         *      },
         *           .
         *           .
         *      label_nameN: {..}
         * }
         *
         * TS:tags[label_name][label_value] = vector[tts_record]
         *
         * This way we end up with lot of redundant data but we're really just
         * using pointers all around, so not much of a deal on the memory
         */
        for (int j = 0; j < pa->points[i].labels_len; ++j) {
            HASH_FIND_STR(ts->tags, (char *)
                          pa->points[i].labels[j].label, dummy);
            if (dummy) {
                /*
                 * Case 1, the label name is already in the outer map, we just
                 * have to make sure the label value is tracked as well
                 */
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
                /*
                 * Case 2, neither the label name nor the label value is stored
                 * into the timeseries tag map, we just insert them
                 */
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
        // Update the columns vector with the latest record created
        TTS_VECTOR_APPEND(ts->columns, record);
    }
    /* Set up the response to the client */
    TTS_SET_RESPONSE_HEADER(&response, TTS_ACK, rc);
    buf->size = pack_tts_packet(&response, (uint8_t *) buf->buf);
    return TTS_OK;
}

/*
 * Auxiliary function used to fill query_responses fields reading data from a
 * single column index on a timeseries
 */
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

/*
 * Fill a query_response with all columns present in a timeseries, uses
 * basically call `handle_tts_query_single` in a loop, producing a *very large*
 * packet
 */
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

/*
 * Ausxiliary function used to retrieve the indexes inside the timeseries
 * vectors, use binary search to find higher and lower indexes representing
 * a range of points
 */
static void get_range_indexes(const struct tts_timeseries *ts,
                              size_t minor_of, size_t major_of,
                              size_t *lo_idx, size_t *hi_idx) {
    TTS_VECTOR_BINSEARCH(ts->timestamps, major_of, lo_idx);
    TTS_VECTOR_BINSEARCH(ts->timestamps, minor_of, hi_idx);
    --*hi_idx;
    if (*lo_idx > 0 && *hi_idx < TTS_VECTOR_SIZE(ts->timestamps)) {
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
}

/*
 * Query a range of points based on timestamps received. Timestamps are assumed
 * to be in nanoseconds
 */
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

/*
 * Just fill the query_response packet with a single point from the timeseries
 * usually needed for FIRST/LAST query results
 */
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

/*
 * Aggregate results into a query_response packet by applying a time-window
 * average, from the starting timestamp to the last covered into the mean
 * bounds
 */
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
        /*
         * We want to "squash" all points in the window range into a single
         * average one
         */
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

/*
 * As `handle_tts_query_mean` it aggregate points into a series of mean values
 * but this time respecting range boundaries specified by the client request,
 * collecting all inner points on every range-block
 */
static void handle_tts_query_mean_r(const struct tts_timeseries *ts,
                                    struct tts_packet *p,
                                    size_t lo, size_t hi,
                                    unsigned long long start,
                                    unsigned long long window,
                                    ev_buf *buf) {
    struct tts_query_response *q = &p->query_r;
    long double avg = 0.0;
    unsigned long long step = 0LL;
    struct tts_record *record = NULL;
    q->results = NULL;
    size_t i = lo, j = 0, k = 0;
    window *= 1e6;
    /*
     * We want to move as close as possible to the first point, just in case
     * the lower bound of the range is much lower than the first point of the
     * timeseries
     */
    while (TTS_VECTOR_AT(ts->timestamps, lo) > start) {
        if (start + window > TTS_VECTOR_AT(ts->timestamps, lo))
            break;
        start += window;
    }
    step = start;
    while (i < hi) {
        q->results = realloc(q->results, (k + 1) * sizeof(*q->results));
        j = 0;
        step += window;
        avg = 0;
        /*
         * We want to "squash" all points in the window range into a single
         * average one
         */
        for (;TTS_VECTOR_AT(ts->timestamps, i) <= step; ++i) {
            record = TTS_VECTOR_AT(ts->columns, i);
            avg += record->value;
            ++j;
        }
        avg /= j;
        q->results[k].labels_len = 0;
        q->results[k].rc = TTS_OK;
        q->results[k].ts_sec = step / (unsigned long long) 1e9;
        q->results[k].ts_nsec = step % (unsigned long long) 1e9;
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
    struct tts_packet response = {0};
    struct tts_timeseries *ts = NULL;
    char *key = (char *) packet->query.ts_name;
    /*
     * First check that the timeseries doesn't exists already, returning a
     * TTS_ENOTS status code in case, as we end having no points to return
     */
    HASH_FIND_STR(payload->tts_db->timeseries, key, ts);
    if (!ts) {
        TTS_SET_RESPONSE_HEADER(&response, TTS_ACK, TTS_ENOTS);
        buf->size = pack_tts_packet(&response, (uint8_t *) buf->buf);
        return TTS_OK;
    }
    TTS_SET_RESPONSE_HEADER(&response, TTS_QUERY_RESPONSE, TTS_OK);
    if (packet->query.byte == TTS_QUERY_ALL_TIMESERIES ||
        packet->query.byte == TTS_QUERY_ALL_TIMESERIES_AVG) {
        /*
         * In this case we want to return all the keyspace (the points of the
         * timeseries), we just need to check if there's some aggregations
         * requested (like avg or filters)
         */
        if (packet->query.bits.mean == 0) {
            handle_tts_query_all(ts, &response, buf);
        } else {
            size_t hi = TTS_VECTOR_SIZE(ts->timestamps);
            handle_tts_query_mean(ts, &response, 0, hi,
                                  packet->query.mean_val, buf);
        }
    } else {
        /*
         * This branch handle the FIRST LAST and RANGE queries, here as well
         * we want to check for filters or aggregations requested
         */
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
                handle_tts_query_mean_r(ts, &response, lo_idx, hi_idx,
                                        major_of, packet->query.mean_val, buf);
            }
        }
    }
    return TTS_OK;
}

/*
 * Main entry-point of the module, just dispatch the payload to the correct
 * handler, based on the opcode of the request
 */
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
