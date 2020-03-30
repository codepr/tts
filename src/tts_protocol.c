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
#include "pack.h"
#include "tts_protocol.h"

/*
 * Unpack from binary to struct tts_create packet, the form of the packet is
 * pretty simple, we just proceed populating each member with its value from
 * the buffer, starting right after the header byte and the size:
 *
 * |   Bit      |  7  |  6  |  5  |  4  |  3  |  2  |  1  |   0    |
 * |------------|--------------------------------------------------|
 * | Byte 5     |           Time series name len MSB               |
 * | Byte 6     |           Time series name len LSB               |
 * |------------|--------------------------------------------------|
 * | Byte 7     |                                                  |
 * |   .        |               Time series name                   |
 * | Byte N     |                                                  |
 * |------------|--------------------------------------------------|<- Array start
 * | Byte N+1   |                 Field len MSB                    |
 * | Byte N+2   |                 Field len LSB                    |
 * |------------|--------------------------------------------------|
 * | Byte N+3   |                                                  |
 * |   .        |                   Field name                     |
 * | Byte N+M   |                                                  |
 * |____________|__________________________________________________|
 *
 * The last two steps, will be repeated until the expected length of the packet
 * is exhausted.
 */
void unpack_tts_create(uint8_t *buf, size_t len, struct tts_create *c) {
    int64_t val = 0;
    unpack_integer(&buf, 'H', &val);
    c->ts_name_len = val;
    c->ts_name = malloc(c->ts_name_len + 1);
    unpack_bytes(&buf, c->ts_name_len, c->ts_name);
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
 * |   Bit      |  7  |  6  |  5  |  4  |  3  |  2  |  1  |   0    |
 * |------------|--------------------------------------------------|
 * | Byte 5     |           Time series name len MSB               |
 * | Byte 6     |           Time series name len LSB               |
 * |------------|--------------------------------------------------|
 * | Byte 7     |                                                  |
 * |   .        |               Time series name                   |
 * | Byte N     |                                                  |
 * |____________|__________________________________________________|
 */
void unpack_tts_delete(uint8_t *buf, size_t len, struct tts_delete *d) {
    (void) len;
    int64_t val = 0;
    unpack_integer(&buf, 'H', &val);
    d->ts_name_len = val;
    d->ts_name = malloc(d->ts_name_len + 1);
    unpack_bytes(&buf, d->ts_name_len, d->ts_name);
}

/*
 * Unpack from binary to struct tts_addpoints packet, the form of the packet is
 * the following, not that different from the previous
 *
 * |   Bit      |  7  |  6  |  5  |  4  |  3  |  2  |  1  |   0    |
 * |------------|--------------------------------------------------|
 * | Byte 5     |           Time series name len MSB               |
 * | Byte 6     |           Time series name len LSB               |
 * |------------|--------------------------------------------------|
 * | Byte 7     |                                                  |
 * |   .        |               Time series name                   |
 * | Byte N     |                                                  |
 * |------------|--------------------------------------------------|<- Array start
 * | Byte N+1   |                 Field len MSB                    |
 * | Byte N+2   |                 Field len LSB                    |
 * |------------|--------------------------------------------------|
 * | Byte N+3   |                                                  |
 * |   .        |                   Field name                     |
 * | Byte N+M   |                                                  |
 * |------------|--------------------------------------------------|
 * | Byte K     |                 Value len MSB                    |
 * | Byte K+1   |                 Value len LSB                    |
 * |------------|--------------------------------------------------|
 * | Byte K+2   |                                                  |
 * |   .        |           Timestamp seconds component            |
 * | Byte K+10  |                                                  |
 * |------------|--------------------------------------------------|
 * | Byte K+11  |                                                  |
 * |   .        |         Timestamp nanoseconds component          |
 * | Byte K+19  |                                                  |
 * |____________|__________________________________________________|
 *
 * The steps starting at [Array start] will be repeated until the expected
 * length of the packet is exhausted.
 */
void unpack_tts_addpoints(uint8_t *buf, size_t len, struct tts_addpoints *a) {
    int64_t val = 0;
    unpack_integer(&buf, 'H', &val);
    a->ts_name_len = val;
    a->ts_name = malloc(a->ts_name_len + 1);
    unpack_bytes(&buf, a->ts_name_len, a->ts_name);
    len -= sizeof(uint16_t) + a->ts_name_len;
    for (int i = 0; len > 0; ++i) {
        a->points = realloc(a->points, sizeof(*a->points));
        unpack_integer(&buf, 'H', &val);
        a->points[i].field_len = val;
        a->points[i].field = malloc(a->points[i].field_len + 1);
        unpack_bytes(&buf, a->points[i].field_len, a->points[i].field);
        unpack_integer(&buf, 'H', &val);
        a->points[i].value_len = val;
        a->points[i].value = malloc(a->points[i].value_len + 1);
        unpack_bytes(&buf, a->points[i].value_len, a->points[i].value);
        // Update the length remaining after the unpack of the field + value
        // bytes
        len -= sizeof(uint16_t) * 2 +
            a->points[i].field_len + a->points[i].value_len;
        // Unpack the seconds and nanoseconds component of the timestamp
        unpack_integer(&buf, 'Q', &val);
        a->points[i].ts_sec = val;
        unpack_integer(&buf, 'Q', &val);
        a->points[i].ts_nsec = val;
        len -= sizeof(uint64_t) * 2;
        ++a->points_len;
    }
}
