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

#ifndef TTS_H
#define TTS_H

#include <time.h>
#include "uthash.h"
#include "tts_vector.h"

#define TTS_TS_FIELDS_MAX_NUMBER 1 << 8
#define TTS_TS_NAME_MAX_LENGTH   1 << 9

/*
 * Simple record struct, wrap around a column inside the database, defined as a
 * key-val couple alike, though it's used only to describe the value of each
 * column
 */
struct tts_record {
    size_t index;
    long double value;
    unsigned char labels_nr;
    struct {
        char *field;
        void *value;
    } *labels;
};

/*
 * Tags structure, used to track secondary tags defined by label fields on
 * timeseries, each tag carries a multi-level hashmap to track them
 */
struct tts_tag {
    char *tag_name;
    TTS_VECTOR(struct tts_record *) column;
    struct tts_tag *tag;
    UT_hash_handle hh;
};

/*
 * Time series, main data structure to handle the time-series, loosely
 * approachable as a `measurement` concept on influx DB, it carries some basic
 * informations like the name of the series and the data. Data are stored as
 * two paired arrays, one indexing the timestamp of each row, the other being
 * an array of arrays of `tts_record`, this way we can easily store different
 * number of columns for each row, depending on the presence of the data during
 * the insertion.
 * A third array is used to store the fields name, each row (index in the
 * columns array) will be paired with the fields array to retrieve what field
 * it refers to, if present.
 */
struct tts_timeseries {
    size_t fields_nr;
    uint32_t retention;
    char name[TTS_TS_NAME_MAX_LENGTH];
    TTS_VECTOR(unsigned long long) timestamps;
    TTS_VECTOR(struct tts_record *) columns;
    struct tts_tag *tags;
    UT_hash_handle hh;
};

/*
 * Just a general store for all the timeseries, KISS as possible
 */
struct tts_database {
    struct tts_timeseries *timeseries;
};

/*
 * Compare two timespec structure, now timespec is composed of seconds and
 * nanoseconds values of the current CLOCK.
 *
 * It returns an integer following the rules:
 *
 * . -1 t1 is lesser than t2
 * .  0 t1 is equal to t2
 * .  1 t1 is greater than t2
 */
static inline int timespec_compare(const struct timespec *t1,
                                   const struct timespec *t2) {
    if (t1->tv_sec == t2->tv_sec) {
        if (t1->tv_nsec == t2->tv_nsec)
            return 0;
        else if (t1->tv_nsec > t2->tv_nsec)
            return 1;
    } else {
        if (t1->tv_sec > t2->tv_sec)
            return 1;
    }
    return -1;
}

/*
 * Init a timeseries structure pointer, do not pass functions as arguments,
 * they will be evaluated multiple times inside the macro
 */
#define TTS_TIMESERIES_INIT(ts, ts_name, ret) do {                  \
    snprintf((ts)->name, TTS_TS_NAME_MAX_LENGTH, "%s", (ts_name));  \
    (ts)->retention = (ret);                                        \
    TTS_VECTOR_NEW((ts)->timestamps);                               \
    TTS_VECTOR_NEW((ts)->columns);                                  \
    (ts)->tags = NULL;                                              \
} while (0)

/*
 * Destroy a timeseries structure pointer, releasing every memory allocated
 * pointer and vectors, do not pass functions as arguments, they will be
 * evaluated multiple times inside the macro
 */
#define TTS_TIMESERIES_DESTROY(ts) do {                         \
    struct tts_record *record = NULL;                           \
    struct tts_tag *tag, *ttmp, *sub_tag, *sub_tmp;             \
    TTS_VECTOR_DESTROY(ts->timestamps);                         \
    for (size_t i = 0; i < TTS_VECTOR_SIZE(ts->columns); ++i) { \
        record = TTS_VECTOR_AT(ts->columns, i);                 \
        for (size_t j = 0; j < record->labels_nr; ++j) {        \
            free(record->labels[j].field);                      \
            free(record->labels[j].value);                      \
        }                                                       \
        free(record->labels);                                   \
        free(record);                                           \
    }                                                           \
    TTS_VECTOR_DESTROY(ts->columns);                            \
    HASH_ITER(hh, ts->tags, tag, ttmp) {                        \
        HASH_DEL(ts->tags, tag);                                \
        TTS_VECTOR_DESTROY(tag->column);                        \
        HASH_ITER(hh, tag->tag, sub_tag, sub_tmp) {             \
            HASH_DEL(tag->tag, sub_tag);                        \
            TTS_VECTOR_DESTROY(sub_tag->column);                \
            free(sub_tag);                                      \
        }                                                       \
        free(tag);                                              \
    }                                                           \
    free(ts);                                                   \
} while (0)

#endif
