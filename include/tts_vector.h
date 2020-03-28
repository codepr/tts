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

#ifndef TTS_VECTOR_H
#define TTS_VECTOR_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define TTS_VECTOR(type) struct { \
    size_t size;                  \
    size_t capacity;              \
    type *data;                   \
}

#define TTS_VECTOR_INIT(vec, cap) do {               \
    assert((cap) > 0);                               \
    (vec).size = 0;                                  \
    (vec).capacity = (cap);                          \
    (vec).data = calloc((cap), sizeof(*(vec).data)); \
} while (0)

#define TTS_VECTOR_DESTROY(vec) free((vec).data)

#define TTS_VECTOR_SIZE(vec) (vec).size

#define TTS_VECTOR_CAPACITY(vec) (vec).capacity

#define TTS_VECTOR_APPEND(vec, item) do {                       \
    if (TTS_VECTOR_SIZE((vec)) == TTS_VECTOR_CAPACITY((vec))) { \
        (vec).capacity *= 2;                                    \
        (vec).data = realloc((vec).data, (vec).capacity);       \
    }                                                           \
    (vec).data[(vec).size++] = (item);                          \
} while (0)

#define TTS_VECTOR_AT(vec, index) do {           \
    assert((index) > 0 && (index) < (vec).size); \
    (vec).data[(index)];                         \
} while (0)

#define TTS_VECTOR_BINSEARCH(vec, target, cmp, res) do { \
    size_t left = 0, middle = 0, right = (vec).size - 1; \
    int found = 0;                                       \
    while (left <= right) {                              \
        middle = floor((left + right) / 2);              \
        if ((vec).data[middle] < (target)) {             \
            left = middle + 1;                           \
        } else if ((vec).data[middle] > (target)) {      \
            right = middle - 1;                          \
        } else {                                         \
            *(res) = middle;                             \
            found = 1;                                   \
            break;                                       \
        }                                                \
    }                                                    \
    if (found == 0) {                                    \
        *(res) = -1;                                     \
    }                                                    \
} while (0)

#endif
