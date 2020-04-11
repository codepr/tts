/* BSD 2-Clause License
 *
 * Copyright (c) 2020, Andrea Giacomo Baldan All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TTS_CONFIG_H
#define TTS_CONFIG_H

#include <unistd.h>
#include <string.h>
#include <stdbool.h>

// Default parameters

#define VERSION           "0.0.1"
#define DEFAULT_LOG_LEVEL DEBUG
#define DEFAULT_CONF_PATH "/etc/tts/tts.conf"
#define DEFAULT_HOSTNAME  "127.0.0.1"
#define DEFAULT_PORT      19191

#define STREQ(s1, s2, len) strncasecmp(s1, s2, len) == 0 ? true : false

struct tts_config {
    /* llb version <MAJOR.MINOR.PATCH> */
    const char *version;
    /* Logging level, to be set by reading configuration */
    int loglevel;
    /* Log file path */
    char logpath[0xFFF];
    /* TCP backlog size */
    int tcp_backlog;
    /* Application pid */
    pid_t pid;
};

extern struct tts_config *conf;

void tts_config_set_default(void);
void tts_config_print(void);
bool tts_config_load(const char *);
void tts_config_unload(void);

#endif
