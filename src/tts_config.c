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

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/resource.h>
#include "ev_tcp.h"
#include "tts_log.h"
#include "tts_config.h"

/* The main configuration structure */
static struct tts_config config;
struct tts_config *conf;

struct llevel {
    const char *lname;
    int loglevel;
};

static const struct llevel lmap[6] = {
    {"FATAL", FATAL},
    {"DEBUG", DEBUG},
    {"WARNING", WARNING},
    {"ERROR", ERROR},
    {"INFO", INFORMATION},
    {"INFORMATION", INFORMATION}
};

static inline void strip_spaces(char **str) {
    if (!*str) return;
    while (isspace(**str) && **str) ++(*str);
}

/*
 * Read the maximum number of file descriptor that can concurrently be open.
 * This value is normally set to 1024, but can be easily tweaked with `ulimit`
 * command
 */
static long get_fh_soft_limit(void) {
    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit)) {
        log_warning("Failed to get limit: %s", strerror(errno));
        return -1;
    }
    return limit.rlim_cur;
}

/* Parse the integer part of a string, by effectively iterate through it and
   converting the numbers found */
static int parse_int(const char *string) {
    int n = 0;

    while (*string && isdigit(*string)) {
        n = (n * 10) + (*string - '0');
        string++;
    }
    return n;
}

/* Set configuration values based on what is read from the persistent
   configuration on disk */
static void add_config_value(const char *key, const char *value) {

    size_t klen = strlen(key);
    size_t vlen = strlen(value);

    if (STREQ("log_level", key, klen) == true) {
        for (int i = 0; i < 3; i++) {
            if (STREQ(lmap[i].lname, value, vlen) == true)
                config.loglevel = lmap[i].loglevel;
        }
    } else if (STREQ("log_path", key, klen) == true) {
        strcpy(config.logpath, value);
    } else if (STREQ("tcp_backlog", key, klen) == true) {
        int tcp_backlog = parse_int(value);
        config.tcp_backlog = tcp_backlog <= SOMAXCONN ? tcp_backlog : SOMAXCONN;
    } else if (STREQ("unix_socket", key, klen) == true) {
        config.mode = TTS_AF_UNIX;
        strcpy(config.host, value);
    } else if (STREQ("ip_address", key, klen) == true) {
        config.mode = TTS_AF_UNIX;
        strcpy(config.host, value);
    } else if (STREQ("ip_port", key, klen) == true) {
        config.port = atoi(value);
    }
}

static inline void unpack_bytes(char **str, char *dest) {

    if (!str || !dest) return;

    while (!isspace(**str) && **str) *dest++ = *(*str)++;
}

/*
 * Return the 'length' of a positive number, as the number of chars it would
 * take in a string
 */
static inline int number_len(size_t number) {
    int len = 1;
    while (number) {
        len++;
        number /= 10;
    }
    return len;
}

/* Format a memory in bytes to a more human-readable form, e.g. 64b or 18Kb
 * instead of huge numbers like 130230234 bytes */
char *memory_to_string(size_t memory) {

    int numlen = 0;
    int translated_memory = 0;

    char *mstring = NULL;

    if (memory < 1024) {
        translated_memory = memory;
        numlen = number_len(translated_memory);
        // +1 for 'b' +1 for nul terminating
        mstring = malloc(numlen + 1);
        snprintf(mstring, numlen + 1, "%db", translated_memory);
    } else if (memory < 1048576) {
        translated_memory = memory / 1024;
        numlen = number_len(translated_memory);
        // +2 for 'Kb' +1 for nul terminating
        mstring = malloc(numlen + 2);
        snprintf(mstring, numlen + 2, "%dKb", translated_memory);
    } else if (memory < 1073741824) {
        translated_memory = memory / (1024 * 1024);
        numlen = number_len(translated_memory);
        // +2 for 'Mb' +1 for nul terminating
        mstring = malloc(numlen + 2);
        snprintf(mstring, numlen + 2, "%dMb", translated_memory);
    } else {
        translated_memory = memory / (1024 * 1024 * 1024);
        numlen = number_len(translated_memory);
        // +2 for 'Gb' +1 for nul terminating
        mstring = malloc(numlen + 2);
        snprintf(mstring, numlen + 2, "%dGb", translated_memory);
    }

    return mstring;
}

bool tts_config_load(const char *configpath) {

    assert(configpath);

    FILE *fh = fopen(configpath, "r");

    if (!fh) {
        log_warning("WARNING: Unable to open conf file %s: %s",
                    configpath, strerror(errno));
        log_warning("To specify a config file run tts -c /path/to/conf");
        return false;
    }

    char line[0xFFF], key[0xFF], value[0xFFF];
    int linenr = 0;
    char *pline, *pkey, *pval;

    while (fgets(line, 0xFFF, fh) != NULL) {

        memset(key, 0x00, 0xFF);
        memset(value, 0x00, 0xFFF);

        linenr++;

        // Skip comments or empty lines
        if (line[0] == '#') continue;

        // Remove whitespaces if any before the key
        pline = line;
        strip_spaces(&pline);

        if (*pline == '\0') continue;

        // Read key
        pkey = key;
        unpack_bytes(&pline, pkey);

        // Remove whitespaces if any after the key and before the value
        strip_spaces(&pline);

        // Ignore eventually incomplete configuration, but notify it
        if (line[0] == '\0') {
            log_warning("WARNING: Incomplete configuration '%s' at line %d. "
                        "Fallback to default.", key, linenr);
            continue;
        }

        // Read value
        pval = value;
        unpack_bytes(&pline, pval);

        // At this point we have key -> value ready to be ingested on the
        // global configuration object
        add_config_value(key, value);
    }

    return true;
}

void tts_config_set_default(void) {

    // Set the global pointer
    conf = &config;

    // Set default values
    config.version = VERSION;
    config.loglevel = DEFAULT_LOG_LEVEL;
    strcpy(config.logpath, DEFAULT_LOG_PATH);
    config.tcp_backlog = SOMAXCONN;
    config.pid = getpid();
    config.mode = DEFAULT_MODE;
    config.port = DEFAULT_PORT;
    strcpy(config.host, DEFAULT_HOSTNAME);
}

void tts_config_print(void) {
    const char *llevel = NULL;
    for (int i = 0; i < 4; i++) {
        if (lmap[i].loglevel == config.loglevel)
            llevel = lmap[i].lname;
    }
    log_info("tts v%s is starting", VERSION);
    log_info("Network settings:");
    log_info("\tSocket family: %s", config.mode == TTS_AF_INET ? "TCP" : "UNIX");
    log_info("\tListening on: %s:%i", config.host, config.port);
    log_info("\tTcp backlog: %d", config.tcp_backlog);
    log_info("\tFile handles soft limit: %li", get_fh_soft_limit());
    log_info("Logging:");
    log_info("\tlevel: %s", llevel);
    if (config.logpath[0])
        log_info("\tlogpath: %s", config.logpath);
    log_info("Event loop backend: %s", EVENTLOOP_BACKEND);
}
