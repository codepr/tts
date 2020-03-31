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

#include <string.h>
#include "pack.h"

// Beej'us network guide functions

/*
 * packi16() -- store a 16-bit int into a char buffer (like htons())
 */
void packi16(uint8_t *buf, uint16_t val) {
    *buf++ = val >> 8;
    *buf++ = val;
}

/*
 * packi32() -- store a 32-bit int into a char buffer (like htonl())
 */
void packi32(uint8_t *buf, uint32_t val) {
    *buf++ = val >> 24;
    *buf++ = val >> 16;
    *buf++ = val >> 8;
    *buf++ = val;
}

/*
 * packi64() -- store a 64-bit int into a char buffer (like htonl()),
 * endiannes-agnostic
 */
void packi64(uint8_t *buf, uint64_t val) {
    *buf++ = val >> 56; *buf++ = val >> 48;
    *buf++ = val >> 40; *buf++ = val >> 32;
    *buf++ = val >> 24; *buf++ = val >> 16;
    *buf++ = val >> 8;  *buf++ = val;
}

/*
 * unpacku16() -- unpack a 16-bit unsigned from a char buffer (like ntohs())
 * endiannes-agnostic
 */
uint32_t unpacku16(uint8_t *buf) {
    return ((uint32_t) buf[0] << 8) | buf[1];
}

/*
 * unpacki16() -- unpack a 16-bit int from a char buffer (like ntohs())
 * endiannes-agnostic
 */
int32_t unpacki16(uint8_t *buf) {
    uint32_t i2 = unpacku16(buf);
    int32_t val;

    // change unsigned numbers to signed
    if (i2 <= 0x7fffu)
        val = i2;
    else
        val = -1 - (uint32_t) (0xffffu - i2);

    return val;
}

/*
 * unpacku32() -- unpack a 32-bit unsigned from a char buffer (like ntohl())
 * endiannes-agnostic
 */
uint64_t unpacku32(uint8_t *buf) {
    return ((uint64_t) buf[0] << 24) |
        ((uint64_t) buf[1] << 16) |
        ((uint64_t) buf[2] << 8)  |
        buf[3];
}

/*
 * unpacki32() -- unpack a 32-bit int from a char buffer (like ntohl())
 * endiannes-agnostic
 */
int64_t unpacki32(uint8_t *buf) {
    uint64_t i2 = unpacku32(buf);
    int64_t val;

    // change unsigned numbers to signed
    if (i2 <= 0x7fffffffu)
        val = i2;
    else
        val = -1 - (int64_t) (0xffffffffu - i2);

    return val;
}

/*
 * unpacku64() -- unpack a 64-bit unsigned from a char buffer (like ntohl())
 * endiannes-agnostic
 */
uint64_t unpacku64(uint8_t *buf) {
    return ((uint64_t) buf[0] << 56) |
        ((uint64_t) buf[1] << 48) |
        ((uint64_t) buf[2] << 40) |
        ((uint64_t) buf[3] << 32) |
        ((uint64_t) buf[4] << 24) |
        ((uint64_t) buf[5] << 16) |
        ((uint64_t) buf[6] << 8)  |
        buf[7];
}

/*
 * unpacki64() -- unpack a 64-bit int from a char buffer (like ntohl())
 * endiannes-agnostic
 */
int64_t unpacki64(uint8_t *buf) {
    uint64_t i2 = unpacku64(buf);
    int64_t val;

    // change unsigned numbers to signed
    if (i2 <= 0x7fffffffffffffffu)
        val = i2;
    else
        val = -1 -(int64_t) (0xffffffffffffffffu - i2);

    return val;
}

/*
 * Unpack integer numbers based on the type on a pointer accepting the result
 * to maintain the same API's definition with `unpack_bytes`, type argument
 * follows the table:
 *
 *   bits | signed   unsigned
 *   -----+-------------------
 *      8 |    b        B
 *     16 |    h        H
 *     32 |    i        I
 *     64 |    q        Q
 */
void unpack_integer(uint8_t **buf, int8_t type, int64_t *val) {
    switch (type) {
        case 'b':
            *val = **buf;
            *buf += 1;
            break;
        case 'B':
            *val = **buf;
            *buf += 1;
            break;
        case 'h':
            *val = unpacki16(*buf);
            *buf += 2;
            break;
        case 'H':
            *val = unpacku16(*buf);
            *buf += 2;
            break;
        case 'i':
            *val = unpacki32(*buf);
            *buf += 4;
            break;
        case 'I':
            *val = unpacku32(*buf);
            *buf += 4;
            break;
        case 'q':
            *val = unpacki64(*buf);
            *buf += 8;
            break;
        case 'Q':
            *val = unpacku64(*buf);
            *buf += 8;
            break;
    }
}

/*
 * Unpack `len` bytes from the buffer into a destination buffer, it's up to
 * the caller making sure the `dst` argument points to a valid buffer
 * (non-NULL) and that it's large enough to store all the content of the `buf`
 * anyway a nul char will be added at the end.
 */
void unpack_bytes(uint8_t **buf, size_t len, uint8_t *dst) {
    memcpy(dst, *buf, len);
    dst[len] = '\0';
    *buf += len;
}
