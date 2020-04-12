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

#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "tts_log.h"
#include "tts_config.h"

/* Global file handle for logging on disk */
static FILE *fh = NULL;

/*
 * Tries to open in append mode a file on disk, to be called only if logging to
 * disk is active
 */
void tts_log_init(const char *file) {
    if (!file) return;
    fh = fopen(file, "a+");
    if (!fh)
        log_warning("WARNING: Unable to open file %s: %s",
                    file, strerror(errno));
}

/*
 * Close the previously opened file handler, to be called only after
 * tts_log_init has been called
 */
void tts_log_close(void) {
    if (fh) {
        fflush(fh);
        fclose(fh);
    }
}

void tts_log(int level, const char *fmt, ...) {

    if (level < conf->loglevel)
        return;

    assert(fmt);

    va_list ap;
    char msg[MAX_LOG_SIZE + 4];
    char timestr[32];
    time_t now = time(NULL);
    struct tm *utcnow = gmtime(&now);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int pos = strftime(timestr, sizeof(timestr), "%F %T.", utcnow);
    snprintf(timestr + pos,sizeof(timestr) - pos, "%03d", (int) tv.tv_usec/1000);

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    /* Truncate message too long and copy 3 bytes to make space for 3 dots */
    memcpy(msg + MAX_LOG_SIZE, "...", 3);
    msg[MAX_LOG_SIZE + 3] = '\0';

    // Open two handler, one for standard output and a second for the
    // persistent log file
    FILE *fp = stdout;

    if (!fp)
        return;

    fprintf(fp, "[%i %s] %s\n", conf->pid, timestr, msg);
    if (fh)
        fprintf(fh, "[%i %s] %s\n", conf->pid, timestr, msg);

    if (level == FATAL)
        exit(EXIT_FAILURE);
}
