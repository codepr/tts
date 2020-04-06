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

#define TTS_VECTOR_BASE_SIZE 4

#define TTS_VECTOR(T) struct { \
    size_t size;               \
    size_t capacity;           \
    T *data;                   \
}

#define TTS_VECTOR_INIT(vec, cap) do {                  \
    assert((cap) > 0);                                  \
    (vec).size = 0;                                     \
    (vec).capacity = (cap);                             \
    (vec).data = calloc((cap), sizeof((vec).data[0]));  \
} while (0)

#define TTS_VECTOR_NEW(vec) \
    TTS_VECTOR_INIT((vec), TTS_VECTOR_BASE_SIZE)

#define TTS_VECTOR_DESTROY(vec) free((vec).data)

#define TTS_VECTOR_SIZE(vec) (vec).size

#define TTS_VECTOR_CAPACITY(vec) (vec).capacity

#define TTS_VECTOR_APPEND(vec, item) do {                               \
    if (TTS_VECTOR_SIZE((vec)) + 1 == TTS_VECTOR_CAPACITY((vec))) {     \
        (vec).capacity *= 2;                                            \
        (vec).data = realloc((vec).data,                                \
                             (vec).capacity * sizeof((vec).data[0]));   \
    }                                                                   \
    (vec).data[(vec).size++] = (item);                                  \
} while (0)

#define TTS_VECTOR_AT(vec, index) (vec).data[(index)]

#define TTS_VECTOR_BINSEARCH(vec, target, res) do {          \
    if ((vec).data[0] >= (target)) {                         \
        *(res) = 0;                                          \
    } else if ((vec).data[(vec).size-1] < (target)) {        \
        *(res) = (vec).size - 1;                             \
    } else {                                                 \
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
            *(res) = left;                                   \
        }                                                    \
    }                                                        \
} while (0)

#define TTS_VECTOR_BINSEARCH_PTR(vec, target, cmp, res) do {       \
    if ((cmp)(&(vec).data[0], (target)) >= 0) {                    \
        *(res) = 0;                                                \
    } else if ((cmp)(&(vec).data[(vec).size-1], (target)) <= 0) {  \
        *(res) = (vec).size - 1;                                   \
    } else {                                                       \
        size_t left = 0, middle = 0, right = (vec).size - 1;       \
        int found = 0;                                             \
        while (left <= right) {                                    \
            middle = floor((left + right) / 2);                    \
            if ((cmp)(&(vec).data[middle], (target)) < 0) {        \
                left = middle + 1;                                 \
            } else if ((cmp)(&(vec).data[middle], (target)) > 0) { \
                right = middle - 1;                                \
            } else {                                               \
                *(res) = middle;                                   \
                found = 1;                                         \
                break;                                             \
            }                                                      \
        }                                                          \
        if (found == 0) {                                          \
            *(res) = left;                                         \
        }                                                          \
    }                                                              \
} while (0)

#endif
