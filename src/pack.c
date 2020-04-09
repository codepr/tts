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

#include <ctype.h>
#include <stdarg.h>
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
uint16_t unpacku16(uint8_t *buf) {
    return ((uint16_t) buf[0] << 8) | buf[1];
}

/*
 * unpacki16() -- unpack a 16-bit int from a char buffer (like ntohs())
 * endiannes-agnostic
 */
int16_t unpacki16(uint8_t *buf) {
    uint16_t i2 = unpacku16(buf);
    int16_t val;

    // change unsigned numbers to signed
    if (i2 <= 0x7fffu)
        val = i2;
    else
        val = -1 - (uint16_t) (0xffffu - i2);

    return val;
}

/*
 * unpacku32() -- unpack a 32-bit unsigned from a char buffer (like ntohl())
 * endiannes-agnostic
 */
uint32_t unpacku32(uint8_t *buf) {
    return ((uint32_t) buf[0] << 24) |
        ((uint32_t) buf[1] << 16) |
        ((uint32_t) buf[2] << 8)  |
        buf[3];
}

/*
 * unpacki32() -- unpack a 32-bit int from a char buffer (like ntohl())
 * endiannes-agnostic
 */
int32_t unpacki32(uint8_t *buf) {
    uint32_t i2 = unpacku32(buf);
    int32_t val;

    // change unsigned numbers to signed
    if (i2 <= 0x7fffffffu)
        val = i2;
    else
        val = -1 - (int32_t) (0xffffffffu - i2);

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
 * Thanks beej.us guide, check https://beej.us/guide/bgnet/html for more useful
 * informations
 */

#define pack754_16(f) (pack754((f), 16, 8))
#define pack754_32(f) (pack754((f), 32, 8))
#define pack754_64(f) (pack754((f), 64, 11))
#define unpack754_16(i) (unpack754((i), 16, 8))
#define unpack754_32(i) (unpack754((i), 32, 8))
#define unpack754_64(i) (unpack754((i), 64, 11))

uint64_t pack754(long double f, unsigned bits, unsigned expbits) {
    long double fnorm;
    int shift;
    long long sign, exp, significand;
    unsigned significandbits = bits - expbits - 1; // -1 for sign bit

    if (f == 0.0) return 0; // get this special case out of the way

    // check sign and begin normalization
    if (f < 0) { sign = 1; fnorm = -f; }
    else { sign = 0; fnorm = f; }

    // get the normalized form of f and track the exponent
    shift = 0;
    while (fnorm >= 2.0) { fnorm /= 2.0; shift++; }
    while (fnorm < 1.0) { fnorm *= 2.0; shift--; }
    fnorm = fnorm - 1.0;

    // calculate the binary form (non-float) of the significand data
    significand = fnorm * ((1LL<<significandbits) + 0.5f);

    // get the biased exponent
    exp = shift + ((1<<(expbits-1)) - 1); // shift + bias

    // return the final answer
    return (sign<<(bits-1)) | (exp<<(bits-expbits-1)) | significand;
}

long double unpack754(uint64_t i, unsigned bits, unsigned expbits) {
    long double result;
    long long shift;
    unsigned bias;
    unsigned significandbits = bits - expbits - 1; // -1 for sign bit

    if (i == 0) return 0.0;

    // pull the significand
    result = (i&((1LL<<significandbits)-1)); // mask
    result /= (1LL<<significandbits); // convert back to float
    result += 1.0f; // add the one back on

    // deal with the exponent
    bias = (1<<(expbits-1)) - 1;
    shift = ((i>>significandbits)&((1LL<<expbits)-1)) - bias;
    while (shift > 0) { result *= 2.0; shift--; }
    while (shift < 0) { result /= 2.0; shift++; }

    // sign it
    result *= (i>>(bits-1))&1? -1.0: 1.0;

    return result;
}

/*
 * Pack integer numbers based on the type on a pointer accepting the result
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
size_t pack_integer(uint8_t **buf, int8_t type, int64_t val) {
    size_t len = 0;
    switch (type) {
        case 'b':
        case 'B':
            **buf = val;
            *buf += 1;
            len = 1;
            break;
        case 'h':
        case 'H':
            packi16(*buf, val);
            *buf += 2;
            len = 2;
            break;
        case 'i':
        case 'I':
            packi32(*buf, val);
            *buf += 4;
            len = 4;
            break;
        case 'q':
        case 'Q':
            packi64(*buf, val);
            *buf += 8;
            len = 8;
            break;
    }
    return len;
}

size_t pack_real(uint8_t **buf, int8_t type, long double val) {
    size_t size = 0;
    uintmax_t fhold;
    switch (type) {
        case 'f': // float-16
            fhold = pack754_16(val); // convert to IEEE 754
            packi16(*buf, fhold);
            buf += 2;
            size += 2;
            break;

        case 'd': // float-32
            fhold = pack754_32(val); // convert to IEEE 754
            packi32(*buf, fhold);
            buf += 4;
            size += 4;
            break;

        case 'g': // float-64
            fhold = pack754_64(val); // convert to IEEE 754
            packi64(*buf, fhold);
            buf += 8;
            size += 8;
            break;
    }
    return size;
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
size_t unpack_integer(uint8_t **buf, int8_t type, int64_t *val) {
    size_t len = 0;
    switch (type) {
        case 'b':
        case 'B':
            *val = **buf;
            *buf += 1;
            len = 1;
            break;
        case 'h':
            *val = unpacki16(*buf);
            *buf += 2;
            len = 2;
            break;
        case 'H':
            *val = unpacku16(*buf);
            *buf += 2;
            len = 2;
            break;
        case 'i':
            *val = unpacki32(*buf);
            *buf += 4;
            len = 4;
            break;
        case 'I':
            *val = unpacku32(*buf);
            *buf += 4;
            len = 4;
            break;
        case 'q':
            *val = unpacki64(*buf);
            *buf += 8;
            len = 8;
            break;
        case 'Q':
            *val = unpacku64(*buf);
            *buf += 8;
            len = 8;
            break;
    }
    return len;
}

size_t unpack_real(uint8_t **buf, int8_t type, long double *val) {
    size_t size = 0;
    uintmax_t fhold;
    switch (type) {
        case 'f': // float
            fhold = unpacku16(*buf);
            *val = unpack754_16(fhold);
            buf += 2;
            size += 2;
            break;

        case 'd': // float-32
            fhold = unpacku32(*buf);
            *val = unpack754_32(fhold);
            buf += 4;
            size += 4;
            break;

        case 'g': // float-64
            fhold = unpacku64(*buf);
            *val = unpack754_64(fhold);
            buf += 8;
            size += 8;
            break;
    }
    return size;
}

/*
 * Unpack `len` bytes from the buffer into a destination buffer, it's up to
 * the caller making sure the `dst` argument points to a valid buffer
 * (non-NULL) and that it's large enough to store all the content of the `buf`
 * anyway a nul char will be added at the end.
 */
size_t unpack_bytes(uint8_t **buf, size_t len, uint8_t *dst) {
    memcpy(dst, *buf, len);
    dst[len] = '\0';
    *buf += len;
    return len;
}

/*
 * unpack() -- unpack data dictated by the format string into the buffer
 *
 *   bits |signed   unsigned   float   string
 *   -----+----------------------------------
 *      8 |   b        B
 *     16 |   h        H         f
 *     32 |   i        I         d
 *     64 |   q        Q         g
 *      - |                               s
 *
 *  (string is extracted based on its stored length, but 's' can be
 *  prepended with a max length)
 */
uint64_t unpack(uint8_t *buf, char *format, ...) {
    va_list ap;

    int8_t *b;              // 8-bit
    uint8_t *B;

    int16_t *h;                      // 16-bit
    uint16_t *H;

    int32_t *i;                 // 32-bit
    uint32_t *I;

    int64_t *q;            // 64-bit
    uint64_t *Q;

    float *f;                    // floats
    double *d;
    long double *g;
    uintmax_t fhold;

    char *s;
    uint64_t maxstrlen = 0, size = 0;

    va_start(ap, format);

    for(; *format != '\0'; format++) {
        switch(*format) {
            case 'b': // 8-bit
                b = va_arg(ap, int8_t *);
                if (*buf <= 0x7f)
                    *b = *buf; // re-sign
                else
                    *b = -1 - (uint8_t) (0xffu - *buf);
                buf++;
                size += 1;
                break;

            case 'B': // 8-bit unsigned
                B = va_arg(ap, uint8_t *);
                *B = *buf++;
                size += 1;
                break;

            case 'h': // 16-bit
                h = va_arg(ap, int16_t *);
                *h = unpacki16(buf);
                buf += 2;
                size += 2;
                break;

            case 'H': // 16-bit unsigned
                H = va_arg(ap, uint16_t *);
                *H = unpacku16(buf);
                buf += 2;
                size += 2;
                break;

            case 'i': // 32-bit
                i = va_arg(ap, int32_t *);
                *i = unpacki32(buf);
                buf += 4;
                size += 4;
                break;

            case 'I': // 32-bit unsigned
                I = va_arg(ap, uint32_t *);
                *I = unpacku32(buf);
                buf += 4;
                size += 4;
                break;

            case 'q': // 64-bit
                q = va_arg(ap, int64_t *);
                *q = unpacki64(buf);
                buf += 8;
                size += 8;
                break;

            case 'Q': // 64-bit unsigned
                Q = va_arg(ap, uint64_t *);
                *Q = unpacku64(buf);
                buf += 8;
                size += 8;
                break;

            case 'f': // float
                f = va_arg(ap, float *);
                fhold = unpacku16(buf);
                *f = unpack754_16(fhold);
                buf += 2;
                size += 2;
                break;

            case 'd': // float-32
                d = va_arg(ap, double *);
                fhold = unpacku32(buf);
                *d = unpack754_32(fhold);
                buf += 4;
                size += 4;
                break;

            case 'g': // float-64
                g = va_arg(ap, long double *);
                fhold = unpacku64(buf);
                *g = unpack754_64(fhold);
                buf += 8;
                size += 8;
                break;

            case 's': // string
                s = va_arg(ap, char *);
                memcpy(s, buf, maxstrlen);
                s[maxstrlen] = '\0';
                buf += maxstrlen;
                size += maxstrlen;
                break;

            default:
                if (isdigit(*format))  // track max str len
                    maxstrlen = maxstrlen * 10 + (*format-'0');
        }

        if (!isdigit(*format))
            maxstrlen = 0;
    }

    va_end(ap);

    return size;
}

/*
 * pack() -- store data dictated by the format string in the buffer
 *
 *   bits |signed   unsigned   float   string
 *   -----+----------------------------------
 *      8 |   b        B
 *     16 |   h        H         f
 *     32 |   i        I         d
 *     64 |   q        Q         g
 *      - |                               s
 *
 *  (16-bit unsigned length is automatically prepended to strings)
 */
uint64_t pack(uint8_t *buf, char *format, ...) {
    va_list ap;

    int8_t b;              // 8-bit
    uint8_t B;

    int16_t h;                      // 16-bit
    uint16_t H;

    int32_t i;                 // 32-bit
    uint32_t I;

    int64_t q;            // 64-bit
    uint64_t Q;

    float f;                    // floats
    double d;
    long double g;
    uintmax_t fhold;

    char *s;                    // strings
    uint64_t len, size = 0;

    va_start(ap, format);

    for(; *format != '\0'; format++) {
        switch(*format) {
            case 'b': // 8-bit
                size += 1;
                b = (int8_t) va_arg(ap, int32_t); // promoted
                *buf++ = b;
                break;

            case 'B': // 8-bit unsigned
                size += 1;
                B = (uint8_t) va_arg(ap, uint32_t); // promoted
                *buf++ = B;
                break;

            case 'h': // 16-bit
                size += 2;
                h = va_arg(ap, int32_t);
                packi16(buf, h);
                buf += 2;
                break;

            case 'H': // 16-bit unsigned
                size += 2;
                H = va_arg(ap, uint32_t);
                packi16(buf, H);
                buf += 2;
                break;

            case 'i': // 32-bit
                size += 4;
                i = va_arg(ap, int32_t);
                packi32(buf, i);
                buf += 4;
                break;

            case 'I': // 32-bit unsigned
                size += 4;
                I = va_arg(ap, uint32_t);
                packi32(buf, I);
                buf += 4;
                break;

            case 'q': // 64-bit
                size += 8;
                q = va_arg(ap, int64_t);
                packi64(buf, q);
                buf += 8;
                break;

            case 'Q': // 64-bit unsigned
                size += 8;
                Q = va_arg(ap, uint64_t);
                packi64(buf, Q);
                buf += 8;
                break;

            case 'f': // float-16
                size += 2;
                f = (float) va_arg(ap, double); // promoted
                fhold = pack754_16(f); // convert to IEEE 754
                packi16(buf, fhold);
                buf += 2;
                break;

            case 'd': // float-32
                size += 4;
                d = va_arg(ap, double);
                fhold = pack754_32(d); // convert to IEEE 754
                packi32(buf, fhold);
                buf += 4;
                break;

            case 'g': // float-64
                size += 8;
                g = va_arg(ap, long double);
                fhold = pack754_64(g); // convert to IEEE 754
                packi64(buf, fhold);
                buf += 8;
                break;

            case 's': // string
                s = va_arg(ap, char *);
                len = strlen(s);
                size += len;
                memcpy(buf, s, len);
                buf += len;
                break;
        }
    }

    va_end(ap);

    return size;
}
