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
#include "tts.h"

int main(void) {
    struct tts_time_series ts;
    TTS_VECTOR_INIT(ts.timestamps, 4);
    TTS_VECTOR_INIT(ts.columns, 4);
    TTS_VECTOR_APPEND(ts.timestamps, 1);
    TTS_VECTOR_APPEND(ts.timestamps, 2);
    TTS_VECTOR_APPEND(ts.timestamps, 3);
    TTS_VECTOR_APPEND(ts.timestamps, 4);
    printf("size: %lu\n", TTS_VECTOR_SIZE(ts.timestamps));
    unsigned long n = 0;
    TTS_VECTOR_BINSEARCH(ts.timestamps, 4, NULL, &n);
    printf("%lu\n", n);
    TTS_VECTOR_DESTROY(ts.columns);
    TTS_VECTOR_DESTROY(ts.timestamps);
    return 0;
}
