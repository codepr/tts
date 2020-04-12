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
#include <stdlib.h>
#include <unistd.h>
#include "tts_log.h"
#include "tts_config.h"
#include "tts_server.h"

static const char *flag_description[] = {
    "Print this help",
    "Set a configuration file to load and use",
    "Set the execution mode, the connection to use, accepts tcp|unix",
    "Set an address hostname to listen on",
    "Set a different port other than 19191",
    "Enable all logs, setting log level to DEBUG",
    "Run in daemon mode"
};

static void print_help(const char *me) {
    printf("\ntts v%s Transient Time Series, a lightweight in-memory TSDB\n\n",
           VERSION);
    printf("Usage: %s [-c conf] [-a addr] [-p port] [-m mode] [-v|-d|-h]\n\n", me);
    const char flags[7] = "hcmapvd";
    for (int i = 0; i < 7; ++i)
        printf(" -%c: %s\n", flags[i], flag_description[i]);
    printf("\n");
}

static int modetoi(const char *str) {
    if (strcasecmp(str, "tcp") == 0)
        return TTS_AF_INET;
    if (strcasecmp(str, "unix") == 0)
        return TTS_AF_UNIX;
    return -1;
}

int main(int argc, char **argv) {
    char *confpath = DEFAULT_CONF_PATH, *host = DEFAULT_HOSTNAME;
    int debug = 0, daemon = 0, port = DEFAULT_PORT, mode = DEFAULT_PORT;
    int opt;

    // Set default configuration
    tts_config_set_default();

    while ((opt = getopt(argc, argv, "m:c:a:p:vhd:")) != -1) {
        switch (opt) {
            case 'm':
                mode = modetoi(optarg);
                if (mode == -1) {
                    fprintf(stderr, "Unknown mode '%s'\n", optarg);
                    print_help(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'a':
                host = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'c':
                confpath = optarg;
                break;
            case 'v':
                debug = 1;
                break;
            case 'd':
                daemon = 1;
                break;
            case 'h':
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                print_help(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Override default DEBUG mode
    conf->loglevel = debug == 1 ? DEBUG : WARNING;
    conf->mode = mode;
    conf->port = port;
    strcpy(conf->host, host);

    tts_config_load(confpath);
    tts_log_init(conf->logpath);

    if (daemon == 1)
        tts_daemonize();

    // Print configuration
    tts_config_print();

    tts_start_server(conf->host, conf->port);

    tts_log_close();

    return 0;
}
