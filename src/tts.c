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

#include <stdio.h>
#include <unistd.h>
#include "tts.h"

int main(void) {
    struct tts_time_series ts;
    TTS_VECTOR_INIT(ts.timestamps, 4, sizeof(struct timespec));
    TTS_VECTOR_INIT(ts.columns, 4, sizeof(TTS_VECTOR(struct tts_record)));
    for (int i = 0; i < 4; ++i)
        TTS_VECTOR_NEW(TTS_VECTOR_AT(ts.columns, i), sizeof(struct tts_record));
    struct timespec specs[4] = {0};
    clock_gettime(CLOCK_REALTIME, &specs[0]);
    usleep(5000);
    struct timespec target;
    clock_gettime(CLOCK_REALTIME, &target);
    clock_gettime(CLOCK_REALTIME, &specs[1]);
    clock_gettime(CLOCK_REALTIME, &specs[2]);
    usleep(3500);
    clock_gettime(CLOCK_REALTIME, &specs[3]);
    for (int i = 0; i < 4; ++i)
        TTS_VECTOR_APPEND(ts.timestamps, specs[i]);
    printf("size: %lu\n", TTS_VECTOR_SIZE(ts.timestamps));
    int n;
    TTS_VECTOR_BINSEARCH(ts.timestamps, &target, timespec_compare, &n);
    printf("target %lu %lu\n", target.tv_sec, target.tv_nsec);
    printf("n %i\n", n);
    struct timespec tspec;
    if (n != -1) {
        tspec = TTS_VECTOR_AT(ts.timestamps, n);
        printf("%i -> %lu%lu\n", n, tspec.tv_sec, tspec.tv_nsec);
    }
    for (int i = 0; i < 4; ++i) {
        tspec = TTS_VECTOR_AT(ts.timestamps, i);
        printf("%i -> %lu %lu\n", i, tspec.tv_sec, tspec.tv_nsec);
        TTS_VECTOR_DESTROY(TTS_VECTOR_AT(ts.columns, i));
    }
    TTS_VECTOR_DESTROY(ts.columns);
    TTS_VECTOR_DESTROY(ts.timestamps);
    return 0;
}
