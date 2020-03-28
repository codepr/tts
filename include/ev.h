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

/*
 * EV_H is a single header library that offers a set of APIs to create and
 * handle a very lightweight event-loop based on the most common IO
 * multiplexing implementations available on Unix-based systems:
 *
 * - Linux-based: epoll
 * - BSD-based (osx): kqueue
 * - All around: poll, select
 *
 * By setting a pre-processor macro definition it's possible to force the use
 * of a wanted implementation.
 *
 * #define EPOLL  1   // set to use epoll
 * #define KQUEUE 1   // set to use kqueue
 * #define POLL   1   // set to use poll
 * #define SELECT 1   // set to use select
 *
 * There's another 2 possible tweakable values, the number of events to monitor
 * at once, which is set to 1024 by default
 *
 * #define EVENTLOOP_MAX_EVENTS    1024
 *
 * The timeout in ms to wait before returning on the blocking call that every
 * IO mux implementation accepts, -1 means block forever until new events
 * arrive.
 * #define EVENTLOOP_TIMEOUT       -1
 *
 * Exposed APIs si divided in generic loop management and TCP server helpers as
 * the most common (but not the only) use cases for this event-loop libraries
 * in general.
 * Please refers to `examples` directory to see some common uses, in particular
 * `echo_server.c` and `ping_pong.c` highlight two simple cases where the
 * genric APIs are employed, `ev_tcp_server.c` instead show how to implement a
 * trivial TCP server using helpers.
 */

#ifndef EV_H
#define EV_H

#ifdef __linux__
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 44)
#define EPOLL 1
#define EVENTLOOP_BACKEND "epoll"
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 1, 23)
#define POLL 1
#define EVENTLOOP_BACKEND "poll"
#else
#define SELECT 1
#define EVENTLOOP_BACKEND "select"
#endif

#elif defined(__APPLE__) || defined(__FreeBSD__) \
    || defined(__OpenBSD__) || defined (__NetBSD__)
#define KQUEUE 1
#define EVENTLOOP_BACKEND "kqueue"
#else
#define SELECT 1
#define EVENTLOOP_BACKEND "select"
#endif // __linux__

#include <time.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#ifdef __linux__
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#endif

/*
 * Maximum number of events to monitor at a time, useful for epoll and poll
 * calls, the value represents the length of the events array. Tweakable value.
 */
#define EVENTLOOP_MAX_EVENTS    1024

/*
 * The timeout to wait before returning on the blocking call that every IO mux
 * implementation accepts, -1 means block forever until new events arrive.
 * Tweakable value.
 */
#define EVENTLOOP_TIMEOUT       -1

/*
 * Return codes */
#define EV_OK   0
#define EV_ERR  -1

/*
 * Event types, meant to be OR-ed on a bitmask to define the type of an event
 * which can have multiple traits
 */
enum ev_type {
    EV_NONE       = 0x00,
    EV_READ       = 0x01,
    EV_WRITE      = 0x02,
    EV_DISCONNECT = 0x04,
    EV_EVENTFD    = 0x08,
    EV_TIMERFD    = 0x10,
    EV_CLOSEFD    = 0x20
};

/*
 * Event loop context, carry the expected number of events to be monitored at
 * every cycle and an opaque pointer to the backend used as engine
 * (select | poll | epoll | kqueue).
 * At start, the library tries to infer the multiplexing IO implementation
 * available on the host machine, `void *api` will carry the API-dedicated
 * struct as a rudimentary form of polymorphism.
 * The context is the main handler of the event loop and is meant to be passed
 * around on each callback fired during the execution of the host application.
 *
 * Idealy it should be at most one context per thread, it's the user's duty to
 * take care of locking of possible shared parts and datastrutures but it's
 * definetly a possibility to run multiple theads each one with his own loop,
 * depending on the scenario, use-case by use-case it can be a feasible
 * solutiion.
 */
typedef struct ev_ctx {
    int events_nr;
    int maxfd; // the maximum FD monitored by the event context,
               // events_monitored must be at least maxfd long
    int stop;
    int is_running;
    int maxevents;
    unsigned long long fired_events;
    struct ev *events_monitored;
    void *api; // opaque pointer to platform defined backends
} ev_context;

/*
 * Event struture used as the main carrier of clients informations, it will be
 * tracked by an array in every context created
 */
struct ev {
    int fd;
    int mask;
    void *rdata; // opaque pointer for read callback args
    void *wdata; // opaque pointer for write callback args
    void (*rcallback)(ev_context *, void *); // read callback
    void (*wcallback)(ev_context *, void *); // write callback
};

/*
 * Initialize the ev_context, accepting the number of events to monitor; that
 * value is indicative as if a FD exceeds the cap set the events array will be
 * resized.
 * The first thing done is the initialization of the api pointer according to
 * the Mux IO backend found on the host machine
 */
void ev_init(ev_context *, int);

/*
 * Just check if the ev_context is running, return 0 if it's not running, 1
 * otherwise
 */
int ev_is_running(const ev_context *);

/*
 * By design ev library can instantiate a default `ev_context`, calling
 * `ev_get_ev_context` the first time will create the loop as a singleton,
 * subsequent calls will retrieve the same first context allocated
 */
ev_context *ev_get_ev_context(void);

/*
 * Call custom destroy function based on the api type set up calling `ev_init`
 * and de-allocate all events monitored memory
 */
void ev_destroy(ev_context *);

/*
 * Poll an event context for events, accepts a timeout or block forever,
 * returning only when a list of FDs are ready to either READ, WRITE or TIMER
 * to be executed.
 */
int ev_poll(ev_context *, time_t);

/*
 * Blocks forever in a loop polling for events with ev_poll calls. At every
 * cycle executes callbacks registered with each event
 */
int ev_run(ev_context *);

/*
 * Trigger a stop on a running event, it's meant to be run as an event in a
 * running ev_ctx
 */
void ev_stop(ev_context *);

/*
 * Add a single FD to the underlying backend of the event loop. Equal to
 * ev_fire_event just without an event to be carried. Useful to add simple
 * descritors like a listening socket o message queue FD.
 */
int ev_watch_fd(ev_context *, int, int);

/*
 * Remove a FD from the loop, even tho a close syscall is sufficient to remove
 * the FD from the underlying backend such as EPOLL/SELECT, this call ensure
 * that any associated events is cleaned out an set to EV_NONE
 */
int ev_del_fd(ev_context *, int);

/*
 * Register a new event, semantically it's equal to ev_register_event but
 * it's meant to be used when an FD is not already watched by the event loop.
 * It could be easily integrated in ev_fire_event call but I prefer maintain
 * the samantic separation of responsibilities.
 */
int ev_register_event(ev_context *, int, int,
                      void (*callback)(ev_context *, void *), void *);

int ev_register_cron(ev_context *,
                     void (*callback)(ev_context *, void *),
                     void *,
                     long long, long long);

/*
 * Register a new event for the next loop cycle to a FD. Equal to ev_watch_fd
 * but allow to carry an event object for the next cycle.
 */
int ev_fire_event(ev_context *, int, int,
                  void (*callback)(ev_context *, void *), void *);

#if defined(EPOLL)

/*
 * =========================
 *  Epoll backend functions
 * =========================
 *
 * The epoll_api structure contains the epoll fd and the events array needed to
 * wait on events with epoll_wait(2) blocking call. It's the best multiplexing
 * IO api available on Linux systems and thus the optimal choice.
 */

#include <sys/epoll.h>

struct epoll_api {
    int fd;
    struct epoll_event *events;
};

/*
 * Epoll management function, register a file descriptor to an EPOLL
 * descriptor, to be monitored for read/write events
 */
static int epoll_add(int efd, int fd, int evs, void *data) {

    struct epoll_event ev;
    ev.data.fd = fd;

    // Being ev.data a union, in case of data != NULL, fd will be set to random
    if (data)
        ev.data.ptr = data;

    ev.events = evs | EPOLLHUP | EPOLLERR;

    return epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
}

/*
 * Modify an epoll-monitored descriptor, can be set to EPOLLIN for read and
 * EPOLLOUT for write
 */
static int epoll_mod(int efd, int fd, int evs, void *data) {

    struct epoll_event ev;
    ev.data.fd = fd;

    // Being ev.data a union, in case of data != NULL, fd will be set to random
    if (data)
        ev.data.ptr = data;

    ev.events = evs | EPOLLHUP | EPOLLERR;

    return epoll_ctl(efd, EPOLL_CTL_MOD, fd, &ev);
}

/*
 * Remove a descriptor from an epoll descriptor, making it no-longer monitored
 * for events
 */
static int epoll_del(int efd, int fd) {
    return epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);
}

static void ev_api_init(ev_context *ctx, int events_nr) {
    struct epoll_api *e_api = malloc(sizeof(*e_api));
    e_api->fd = epoll_create1(0);
    e_api->events = calloc(events_nr, sizeof(struct epoll_event));
    ctx->api = e_api;
    ctx->maxfd = events_nr;
}

static void ev_api_destroy(ev_context *ctx) {
    close(((struct epoll_api *) ctx->api)->fd);
    free(((struct epoll_api *) ctx->api)->events);
    free(ctx->api);
}

static int ev_api_get_event_type(ev_context *ctx, int idx) {
    struct epoll_api *e_api = ctx->api;
    int events = e_api->events[idx].events;
    int ev_mask = ctx->events_monitored[e_api->events[idx].data.fd].mask;
    // We want to remember the previous events only if they're not of type
    // CLOSE or TIMER
    int mask = ev_mask & (EV_CLOSEFD|EV_TIMERFD) ? ev_mask : EV_NONE;
    if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) mask |= EV_DISCONNECT;
    if (events & EPOLLIN) mask |= EV_READ;
    if (events & EPOLLOUT) mask |= EV_WRITE;
    return mask;
}

static int ev_api_poll(ev_context *ctx, time_t timeout) {
    struct epoll_api *e_api = ctx->api;
    return epoll_wait(e_api->fd, e_api->events, ctx->events_nr, timeout);
}

static int ev_api_watch_fd(ev_context *ctx, int fd) {
    struct epoll_api *e_api = ctx->api;
    return epoll_add(e_api->fd, fd, EPOLLIN, NULL);
}

static int ev_api_del_fd(ev_context *ctx, int fd) {
    struct epoll_api *e_api = ctx->api;
    return epoll_del(e_api->fd, fd);
}

static int ev_api_register_event(ev_context *ctx, int fd, int mask) {
    struct epoll_api *e_api = ctx->api;
    int op = 0;
    if (mask & EV_READ) op |= EPOLLIN;
    if (mask & EV_WRITE) op |= EPOLLOUT;
    return epoll_add(e_api->fd, fd, op, NULL);
}

static int ev_api_fire_event(ev_context *ctx, int fd, int mask) {
    struct epoll_api *e_api = ctx->api;
    int op = 0;
    if (mask & EV_READ) op |= EPOLLIN;
    if (mask & EV_WRITE) op |= EPOLLOUT;
    if (mask & EV_EVENTFD)
        return epoll_add(e_api->fd, fd, op, NULL);
    return epoll_mod(e_api->fd, fd, op, NULL);
}

/*
 * Get the event on the idx position inside the events map. The event can also
 * be an unset one (EV_NONE)
 */
static inline struct ev *ev_api_fetch_event(const ev_context *ctx,
                                            int idx, int mask) {
    (void) mask; // silence the compiler warning
    int fd = ((struct epoll_api *) ctx->api)->events[idx].data.fd;
    return ctx->events_monitored + fd;
}

#elif defined(POLL)

/*
 * =========================
 *  Poll backend functions
 * =========================
 *
 * The poll_api structure contains the number of fds to monitor and the array
 * of pollfd structures associated. This number must be adjusted everytime a
 * client disconnect or a new connection have an fd > nfds to avoid iterating
 * over closed fds everytime a new event is triggered.
 * As select, poll iterate linearly over all the triggered events, without the
 * hard limit of 1024 connections. It's the second best option available if no
 * epoll or kqueue for Mac OSX are not present.
 */

#include <poll.h>

struct poll_api {
    int nfds;
    int events_monitored;
    struct pollfd *fds;
};

static void ev_api_init(ev_context *ctx, int events_nr) {
    struct poll_api *p_api = malloc(sizeof(*p_api));
    p_api->nfds = 0;
    p_api->fds = calloc(events_nr, sizeof(struct pollfd));
    p_api->events_monitored = events_nr;
    ctx->api = p_api;
    ctx->maxfd = events_nr;
}

static void ev_api_destroy(ev_context *ctx) {
    free(((struct poll_api *) ctx->api)->fds);
    free(ctx->api);
}

static int ev_api_get_event_type(ev_context *ctx, int idx) {
    struct poll_api *p_api = ctx->api;
    int ev_mask = ctx->events_monitored[p_api->fds[idx].fd].mask;
    // We want to remember the previous events only if they're not of type
    // CLOSE or TIMER
    int mask = ev_mask & (EV_CLOSEFD|EV_TIMERFD) ? ev_mask : 0;
    if (p_api->fds[idx].revents & (POLLHUP|POLLERR)) mask |= EV_DISCONNECT;
    if (p_api->fds[idx].revents & POLLIN) mask |= EV_READ;
    if (p_api->fds[idx].revents & POLLOUT) mask |= EV_WRITE;
    return mask;
}

static int ev_api_poll(ev_context *ctx, time_t timeout) {
    struct poll_api *p_api = ctx->api;
    int err = poll(p_api->fds, p_api->nfds, timeout);
    if (err < 0)
        return EV_ERR;
    return p_api->nfds;
}

/*
 * Poll maintain in his state the number of file descriptor it monitor in a
 * fixed size array just like the events we monitor over the primitive. If a
 * resize is needed cause the number of fds have reached the length of the fds
 * array, we must increase its size.
 */
static int ev_api_watch_fd(ev_context *ctx, int fd) {
    struct poll_api *p_api = ctx->api;
    p_api->fds[p_api->nfds].fd = fd;
    p_api->fds[p_api->nfds].events = POLLIN;
    p_api->nfds++;
    if (p_api->nfds >= p_api->events_monitored) {
        p_api->events_monitored *= 2;
        p_api->fds = realloc(p_api->fds,
                                 p_api->events_monitored * sizeof(struct pollfd));
    }
    return EV_OK;
}

static int ev_api_del_fd(ev_context *ctx, int fd) {
    struct poll_api *p_api = ctx->api;
    for (int i = 0; i < p_api->nfds; ++i) {
        if (p_api->fds[i].fd == fd) {
            p_api->fds[i].fd = -1;
            p_api->fds[i].events = 0;
            // Resize fds array
            for(int j = i; j < p_api->nfds-1; ++j)
                p_api->fds[j].fd = p_api->fds[j + 1].fd;
            p_api->nfds--;
            break;
        }
    }
    return EV_OK;
}

/*
 * We have to check for resize even here just like ev_api_watch_fd.
 */
static int ev_api_register_event(ev_context *ctx, int fd, int mask) {
    struct poll_api *p_api = ctx->api;
    p_api->fds[p_api->nfds].fd = fd;
    if (mask & EV_READ) p_api->fds[p_api->nfds].events |= POLLIN;
    if (mask & EV_WRITE) p_api->fds[p_api->nfds].events |= POLLOUT;
    p_api->nfds++;
    if (p_api->nfds >= p_api->events_monitored) {
        p_api->events_monitored *= 2;
        p_api->fds = realloc(p_api->fds,
                                 p_api->events_monitored * sizeof(struct pollfd));
    }
    return EV_OK;
}

static int ev_api_fire_event(ev_context *ctx, int fd, int mask) {
    struct poll_api *p_api = ctx->api;
    for (int i = 0; i < p_api->nfds; ++i) {
        if (p_api->fds[i].fd == fd) {
            p_api->fds[i].events = mask & EV_READ ? POLLIN : POLLOUT;
            break;
        }
    }
    return EV_OK;
}

/*
 * Get the event on the idx position inside the events map. The event can also
 * be an unset one (EV_NONE)
 */
static inline struct ev *ev_api_fetch_event(const ev_context *ctx,
                                            int idx, int mask) {
    return ctx->events_monitored + ((struct poll_api *) ctx->api)->fds[idx].fd;
}

#elif defined(SELECT)

/*
 * ==========================
 *  Select backend functions
 * ==========================
 *
 * The select_api structure contains two copies of the read/write fdset, it's
 * a measure to reset the original monitored fd_set after each select(2) call
 * as it's not safe to iterate over the already selected sets, select(2) make
 * side-effects on the passed in fd_sets.
 * At each new event all the monitored fd_set are iterated and check for read
 * or write readiness; the number of monitored sockets is hard-capped at 1024.
 * It's the oldest multiplexing IO and practically obiquitous, making it the
 * perfect fallback for every system.
 */

// Maximum number of monitorable descriptors on select
#define SELECT_FDS_HARDCAP 1024

struct select_api {
    fd_set rfds, wfds;
    // Copy of the original fdset arrays to re-initialize them after each cycle
    // we'll call them "service" fdset
    fd_set _rfds, _wfds;
};

static void ev_api_init(ev_context *ctx, int events_nr) {
    /*
     * fd_set is an array of 32 i32 and each FD is represented by a bit so
     * 32 x 32 = 1024 as hard limit
     */
    assert(events_nr <= SELECT_FDS_HARDCAP);
    struct select_api *s_api = malloc(sizeof(*s_api));
    FD_ZERO(&s_api->rfds);
    FD_ZERO(&s_api->wfds);
    ctx->api = s_api;
    ctx->maxfd = 0;
}

static void ev_api_destroy(ev_context *ctx) {
    free(ctx->api);
}

static int ev_api_get_event_type(ev_context *ctx, int idx) {
    struct select_api *s_api = ctx->api;
    int ev_mask = ctx->events_monitored[idx].mask;
    // We want to remember the previous events only if they're not of type
    // CLOSE or TIMER
    int mask = ev_mask & (EV_CLOSEFD|EV_TIMERFD) ? ev_mask : 0;
    /*
     * Select checks all FDs by looping to the highest registered FD it
     * currently monitor. Even non set or non monitored FDs are inspected, we
     * have to ensure that the FD is currently ready for IO, otherwise we'll
     * end up looping all FDs and calling callbacks everytime, even when
     * there's no need to.
     *
     * Also we have to check for ready FDs on "service" _fdsets, cause they're
     * the ones employed on the select call to avoid side-effects on the
     * originals.
     */
    if (!FD_ISSET(idx, &s_api->_rfds) && !FD_ISSET(idx, &s_api->_wfds))
        return EV_NONE;
    if (FD_ISSET(idx, &s_api->_rfds)) mask |= EV_READ;
    if (FD_ISSET(idx, &s_api->_wfds)) mask |= EV_WRITE;
    return mask;
}

static int ev_api_poll(ev_context *ctx, time_t timeout) {
    struct timeval *tv =
        timeout > 0 ? &(struct timeval){ 0, timeout * 1000 } : NULL;
    struct select_api *s_api = ctx->api;
    // Re-initialize fdset arrays cause select call side-effect the originals
    memcpy(&s_api->_rfds, &s_api->rfds, sizeof(fd_set));
    memcpy(&s_api->_wfds, &s_api->wfds, sizeof(fd_set));
    int err = select(ctx->maxfd + 1, &s_api->_rfds, &s_api->_wfds, NULL, tv);
    if (err < 0)
        return EV_ERR;
    return ctx->maxfd + 1;
}

static int ev_api_watch_fd(ev_context *ctx, int fd) {
    struct select_api *s_api = ctx->api;
    FD_SET(fd, &s_api->rfds);
    // Check for a possible new max fd, we don't want to miss events on FDs
    if (fd > ctx->maxfd)
        ctx->maxfd = fd;
    return EV_OK;
}

static int ev_api_del_fd(ev_context *ctx, int fd) {
    struct select_api *s_api = ctx->api;
    if (FD_ISSET(fd, &s_api->rfds)) FD_CLR(fd, &s_api->rfds);
    if (FD_ISSET(fd, &s_api->wfds)) FD_CLR(fd, &s_api->wfds);
    /*
     * To remove  FD from select we must determine the new maximum descriptor
     * value based on the bits that are still turned on in the rfds set.
     */
    if (fd == ctx->maxfd) {
        while (!FD_ISSET(ctx->maxfd, &s_api->rfds)
               && !FD_ISSET(ctx->maxfd, &s_api->wfds))
            ctx->maxfd -= 1;
    }
    return EV_OK;
}

static int ev_api_register_event(ev_context *ctx, int fd, int mask) {
    struct select_api *s_api = ctx->api;
    if (mask & EV_READ) FD_SET(fd, &s_api->rfds);
    if (mask & EV_WRITE) FD_SET(fd, &s_api->wfds);
    // Check for a possible new max fd, we don't want to miss events on FDs
    if (fd > ctx->maxfd)
        ctx->maxfd = fd;
    return EV_OK;
}

static int ev_api_fire_event(ev_context *ctx, int fd, int mask) {
    struct select_api *s_api = ctx->api;
    if (mask & EV_READ) FD_SET(fd, &s_api->rfds);
    if (mask & EV_WRITE) FD_SET(fd, &s_api->wfds);
    return EV_OK;
}

/*
 * Get the event on the idx position inside the events map. The event can also
 * be an unset one (EV_NONE)
 */
static inline struct ev *ev_api_fetch_event(const ev_context *ctx,
                                            int idx, int mask) {
    return ctx->events_monitored + idx;
}

#elif defined(KQUEUE)

/*
 * ==========================
 *  Kqueue backend functions
 * ==========================
 *
 * The Epoll counterpart on BSD systems, including Mac OSX, it's the older of
 * the two Mux IO implementations and on par in terms of performances, a bit
 * more versatile as it's possible to schedule timers directly as events
 * instead of relying on support mechanisms like `timerfd` on linux.
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

struct kqueue_api {
    int fd;
    struct kevent *events;
};

static void ev_api_init(ev_context *ctx, int events_nr) {
    struct kqueue_api *k_api = malloc(sizeof(*k_api));
    k_api->fd = kqueue();
    k_api->events = calloc(events_nr, sizeof(struct kevent));
    ctx->api = k_api;
    ctx->maxfd = events_nr;
}

static void ev_api_destroy(ev_context *ctx) {
    close(((struct kqueue_api *) ctx->api)->fd);
    free(((struct kqueue_api *) ctx->api)->events);
    free(ctx->api);
}

static int ev_api_get_event_type(ev_context *ctx, int idx) {
    struct kqueue_api *k_api = ctx->api;
    int events = k_api->events[idx].flags;
    int ev_mask = ctx->events_monitored[k_api->events[idx].ident].mask;
    // We want to remember the previous events only if they're not of type
    // CLOSE or TIMER
    int mask = ev_mask & (EV_CLOSEFD | EV_TIMERFD) ? ev_mask : EV_NONE;
    if (events & (EV_EOF | EV_ERROR)) mask |= EV_DISCONNECT;
    if (events & EVFILT_READ) mask |= EV_READ;
    if (events & EVFILT_WRITE) mask |= EV_WRITE;
    return mask;
}

static int ev_api_poll(ev_context *ctx, time_t timeout) {
    struct kqueue_api *k_api = ctx->api;
    struct timespec ts_timeout;
    ts_timeout.tv_sec = timeout;
    ts_timeout.tv_nsec = 0;
    int err = kevent(k_api->fd, NULL, 0,
                     k_api->events, ctx->maxevents, &ts_timeout);
    if (err < 0)
        return EV_ERR;
    return err;
}

static int ev_api_del_fd(ev_context *ctx, int fd) {
    struct kqueue_api *k_api = ctx->api;
    struct kevent ke;
    int ev_mask = ctx->events_monitored[fd].mask;
    int mask = 0;
    if (ev_mask & EV_READ) mask |= EVFILT_READ;
    if (ev_mask & EV_WRITE) mask |= EVFILT_WRITE;
    if (ev_mask & EV_TIMERFD) mask |= EVFILT_TIMER;
    EV_SET(&ke, fd, mask, EV_DELETE, 0, 0, NULL);
    if (kevent(k_api->fd, &ke, 1, NULL, 0, NULL) == -1)
        return EV_ERR;
    return EV_OK;
}

static int ev_api_register_event(ev_context *ctx, int fd, int mask) {
    struct kqueue_api *k_api = ctx->api;
    struct kevent ke;
    int op = 0;
    if (mask & EV_READ) op |= EVFILT_READ;
    if (mask & EV_WRITE) op |= EVFILT_WRITE;
    EV_SET(&ke, fd, op, EV_ADD, 0, 0, NULL);
    if (kevent(k_api->fd, &ke, 1, NULL, 0, NULL) == -1)
        return EV_ERR;
    return EV_OK;
}

static int ev_api_watch_fd(ev_context *ctx, int fd) {
    return ev_api_register_event(ctx, fd, EV_READ);
}

static int ev_api_fire_event(ev_context *ctx, int fd, int mask) {
    struct kqueue_api *k_api = ctx->api;
    struct kevent ke;
    int op = 0;
    if (mask & (EV_READ | EV_EVENTFD)) op |= EVFILT_READ;
    if (mask & EV_WRITE) op |= EVFILT_WRITE;
    EV_SET(&ke, fd, op, EV_ADD | EV_ENABLE, 0, 0, NULL);
    if (kevent(k_api->fd, &ke, 1, NULL, 0, NULL) == -1)
        return EV_ERR;
    return EV_OK;
}

/*
 * Get the event on the idx position inside the events map. The event can also
 * be an unset one (EV_NONE)
 */
static inline struct ev *ev_api_fetch_event(const ev_context *ctx,
                                            int idx, int mask) {
    (void) mask; // silence compiler warning
    int fd = ((struct kqueue_api *) ctx->api)->events[idx].ident;
    return ctx->events_monitored + fd;
}

#endif // KQUEUE

static ev_context ev_default_ctx;
static int ev_default_ctx_inited = 0;
#ifdef __linux__
static int quit_sig;
#else
static int quit_sig[2];
#endif

// Stops epoll_wait loops by sending an event
static void ev_sigint_handler(int signum) {
    (void) signum;
#ifdef __linux__
    eventfd_write(quit_sig, 1);
#else
    (void) write(quit_sig[0], &(unsigned long) {1}, sizeof(unsigned long));
#endif
}

/*
 * Eventloop stop callback, will be triggered by an EV_CLOSEFD event and stop
 * the running loop, unblocking the call.
 */
static void ev_stop_callback(ev_context *ctx, void *arg) {
    (void) arg;
    ev_stop(ctx);
}

/*
 * Process the event at the position idx in the events_monitored array. Read or
 * write events can be executed on the same iteration, differentiating just
 * on EV_CLOSEFD or EV_EVENTFD.
 * Returns the number of fired callbacks.
 */
static int ev_process_event(ev_context *ctx, int idx, int mask) {
    if (mask == EV_NONE) return EV_OK;
    struct ev *e = ev_api_fetch_event(ctx, idx, mask);
    int err = 0, fired = 0, fd = e->fd;
    if (mask & EV_CLOSEFD) {
#ifdef __linux__
        err = eventfd_read(fd, &(eventfd_t){0});
#else
        err = read(fd, &(unsigned long){0}, sizeof(unsigned long));
#endif // __linux__
        if (err < 0) return EV_OK;
        e->rcallback(ctx, e->rdata);
        ++fired;
    } else {
        if (mask & EV_EVENTFD) {
#ifdef __linux__
            err = eventfd_read(fd, &(eventfd_t){0L});
#else
            err = read(fd, &(unsigned long){0}, sizeof(unsigned long));
#endif // __linux__
            close(fd);
        } else if (mask & EV_TIMERFD) {
            err = read(fd, &(unsigned long int){0L}, sizeof(unsigned long int));
        }
        if (err < 0) return EV_OK;
        if (mask & EV_READ) {
            e->rcallback(ctx, e->rdata);
            ++fired;
        }
        if (mask & EV_WRITE) {
            if (!fired || e->wcallback != e->rcallback) {
                e->wcallback(ctx, e->wdata);
                ++fired;
            }
        }
    }
    return fired;
}

/*
 * Auxiliary function, update FD, mask and data in monitored events array.
 * Monitored events are the same number as the maximum FD registered in the
 * context.
 */
static void ev_add_monitored(ev_context *ctx, int fd, int mask,
                             void (*callback)(ev_context *, void *),
                             void *ptr) {
    /*
     * TODO check for fd <= 1024 if using SELECT
     * That is because FD_SETSIZE is fixed to 1024, fd_set is an array of 32
     * i32 and each FD is represented by a bit so 32 x 32 = 1024 as hard limit
     */
    if (fd > ctx->maxevents) {
        int i = ctx->maxevents;
        ctx->maxevents = fd;
        if (fd > ctx->events_nr) {
            ctx->events_monitored =
                realloc(ctx->events_monitored, (fd + 1) * sizeof(struct ev));
            for (; i < ctx->maxevents; ++i)
                ctx->events_monitored[i].mask = EV_NONE;
        }
    }
    ctx->events_monitored[fd].fd = fd;
    ctx->events_monitored[fd].mask |= mask;
    if (mask & EV_READ) {
        ctx->events_monitored[fd].rdata = ptr;
        ctx->events_monitored[fd].rcallback = callback;
    }
    if (mask & EV_WRITE) {
        ctx->events_monitored[fd].wdata = ptr;
        ctx->events_monitored[fd].wcallback = callback;
    }
}

static inline int ev_get_event_type(ev_context *ctx, int idx) {
    return ev_api_get_event_type(ctx, idx);
}

ev_context *ev_get_ev_context(void) {
    if (ev_default_ctx_inited == 0) {
#ifdef __linux__
        quit_sig = eventfd(0, EFD_NONBLOCK);
#else
        pipe(quit_sig);
#endif
        signal(SIGINT, ev_sigint_handler);
        signal(SIGTERM, ev_sigint_handler);
        ev_init(&ev_default_ctx, EVENTLOOP_MAX_EVENTS);
#ifdef __linux__
        ev_register_event(&ev_default_ctx, quit_sig,
                          EV_CLOSEFD | EV_READ, ev_stop_callback, NULL);
#else
        ev_register_event(&ev_default_ctx, quit_sig[1],
                          EV_CLOSEFD | EV_READ, ev_stop_callback, NULL);
#endif
        ev_default_ctx_inited = 1;
    }
    return &ev_default_ctx;
}

void ev_init(ev_context *ctx, int events_nr) {
    ev_api_init(ctx, events_nr);
    ctx->stop = 0;
    ctx->fired_events = 0;
    ctx->is_running = 0;
    ctx->maxevents = events_nr;
    ctx->events_nr = events_nr;
    ctx->events_monitored = calloc(events_nr, sizeof(struct ev));
}

int ev_is_running(const ev_context *ctx) {
    return ctx->is_running;
}

void ev_destroy(ev_context *ctx) {
    for (int i = 0; i < ctx->maxevents; ++i) {
        if (!(ctx->events_monitored[i].mask & EV_CLOSEFD) &&
            ctx->events_monitored[i].mask != EV_NONE)
            ev_del_fd(ctx, ctx->events_monitored[i].fd);
    }
    ctx->is_running = 0;
    free(ctx->events_monitored);
    ev_api_destroy(ctx);
}

/*
 * Poll an event context for events, accepts a timeout or block forever,
 * returning only when a list of FDs are ready to either READ, WRITE or TIMER
 * to be executed.
 */
int ev_poll(ev_context *ctx, time_t timeout) {
    return ev_api_poll(ctx, timeout);
}

/*
 * Blocks forever in a loop polling for events with ev_poll calls. At every
 * cycle executes callbacks registered with each event
 */
int ev_run(ev_context *ctx) {
    int n = 0, events = 0;
    /*
     * Start an infinite loop, can be stopped only by scheduling an ev_stop
     * callback or if an error on the underlying backend occur
     */
    ctx->is_running = 1;
    while (!ctx->stop) {
        /*
         * blocks polling for events, -1 means forever. Returns only in case of
         * valid events ready to be processed or errors
         */
        n = ev_poll(ctx, EVENTLOOP_TIMEOUT);
        if (n < 0) {
            /* Signals to all threads. Ignore it for now */
            if (errno == EINTR)
                continue;
            /* Error occured, break the loop */
            break;
        }
        for (int i = 0; i < n; ++i) {
            events = ev_get_event_type(ctx, i);
            ctx->fired_events += ev_process_event(ctx, i, events);
        }
    }
    return n;
}

/*
 * Trigger a stop on a running event, it's meant to be run as an event in a
 * running ev_ctx
 */
void ev_stop(ev_context *ctx) {
    ctx->is_running = 0;
    ctx->stop = 1;
}

/*
 * Add a single FD to the underlying backend of the event loop. Equal to
 * ev_fire_event just without an event to be carried. Useful to add simple
 * descritors like a listening socket o message queue FD.
 */
int ev_watch_fd(ev_context *ctx, int fd, int mask) {
    ev_add_monitored(ctx, fd, mask, NULL, NULL);
    return ev_api_watch_fd(ctx, fd);
}

/*
 * Remove a FD from the loop, even tho a close syscall is sufficient to remove
 * the FD from the underlying backend such as EPOLL/SELECT, this call ensure
 * that any associated events is cleaned out an set to EV_NONE
 */
int ev_del_fd(ev_context *ctx, int fd) {
    memset(ctx->events_monitored + fd, 0x00, sizeof(struct ev));
    return ev_api_del_fd(ctx, fd);
}

/*
 * Register a new event, semantically it's equal to ev_register_event but
 * it's meant to be used when an FD is not already watched by the event loop.
 * It could be easily integrated in ev_fire_event call but I prefer maintain
 * the samantic separation of responsibilities.
 *
 * Set a callback and an argument to be passed to for the next loop cycle,
 * associating it to a file descriptor, ultimately resulting in an event to be
 * dispatched and processed.
 *
 * The difference with ev_fire_event is that this function should be called
 * when the file descriptor is not registered in the loop yet.
 *
 * - mask: bitmask used to describe what type of event we're going to fire
 * - callback:  is a function pointer to the routine we want to execute
 * - data:  an opaque pointer to the arguments for the callback.
 */
int ev_register_event(ev_context *ctx, int fd, int mask,
                      void (*callback)(ev_context *, void *), void *data) {
    ev_add_monitored(ctx, fd, mask, callback, data);
    int ret = 0;
    ret = ev_api_register_event(ctx, fd, mask);
    if (ret < 0) return EV_ERR;
    if (mask & EV_EVENTFD)
#ifdef __linux__
        (void) eventfd_write(fd, 1);
#else
        (void) write(fd, &(unsigned long){1}, sizeof(unsigned long));
#endif
    return EV_OK;
}

/*
 * Register a periodically repeate callback and args to be passed to a running
 * loop, specifying, seconds and/or nanoseconds defining how often the callback
 * should be executed.
 */
int ev_register_cron(ev_context *ctx,
                     void (*callback)(ev_context *, void *),
                     void *data,
                     long long s, long long ns) {
#ifdef __linux__
    struct itimerspec timer;
    memset(&timer, 0x00, sizeof(timer));
    timer.it_value.tv_sec = s;
    timer.it_value.tv_nsec = ns;
    timer.it_interval.tv_sec = s;
    timer.it_interval.tv_nsec = ns;

    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

    if (timerfd_settime(timerfd, 0, &timer, NULL) < 0)
        return EV_ERR;

    // Add the timer to the event loop
    ev_add_monitored(ctx, timerfd, EV_TIMERFD|EV_READ, callback, data);
    return ev_api_watch_fd(ctx, timerfd);
#else
    struct kqueue_api *k_api = ctx->api;
    // milliseconds
    unsigned period = (s * 1000)  + (ns / 100);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ev_add_monitored(ctx, fd, EV_TIMERFD|EV_READ, callback, data);
    struct kevent ke;
    EV_SET(&ke, fd, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, period, 0);
    if (kevent(k_api->fd, &ke, 1, NULL, 0, NULL) == -1)
        return EV_ERR;
    return EV_OK;
#endif // __linux__
}

/*
 * Register a new event for the next loop cycle to a FD. Equal to ev_watch_fd
 * but allow to carry an event object for the next cycle.
 *
 * Set a callback and an argument to be passed to for the next loop cycle,
 * associating it to a file descriptor, ultimately resulting in an event to be
 * dispatched and processed.
 *
 * Behave like ev_register_event but it's meant to be called when the file
 * descriptor is already registered in the loop.
 *
 * - mask: bitmask used to describe what type of event we're going to fire
 * - callback:  is a function pointer to the routine we want to execute
 * - data:  an opaque pointer to the arguments for the callback.
 */
int ev_fire_event(ev_context *ctx, int fd, int mask,
                  void (*callback)(ev_context *, void *), void *data) {
    int ret = 0;
    ev_add_monitored(ctx, fd, mask, callback, data);
    ret = ev_api_fire_event(ctx, fd, mask);
    if (ret < 0) return EV_ERR;
    if (mask & EV_EVENTFD) {
#ifdef __linux__
        ret = eventfd_write(fd, 1);
#else
        ret = write(fd, &(unsigned long){1}, sizeof(unsigned long));
#endif // __linux__
        if (ret < 0) return EV_ERR;
    }
    return EV_OK;
}

/*
 * =================================
 *  TCP server helper APIs exposed
 * =================================
 *
 * A set of basic helpers to create a lightweight event-driven TCP server based
 * on non-blocking sockets and IO multiplexing using ev as underlying
 * event-loop.
 *
 * As of now it's stll very simple, the only tweakable value is the buffer
 * memory size for incoming and to-be-written stream of bytes for clients, the
 * default value is 2048.
 *
 * #define EV_TCP_BUFSIZE 2048
 */

#define EV_TCP_SUCCESS           0
#define EV_TCP_FAILURE          -1
#define EV_TCP_MISSING_CALLBACK -2
#define EV_TCP_MISSING_CONTEXT  -3

/*
 * Default buffer size for connecting client, can be changed on the host
 * application
 */
#define EV_TCP_BUFSIZE           2048

typedef struct ev_server ev_tcp_server;
typedef struct tcp_client ev_tcp_client;
typedef struct ev_server ev_udp_server;
typedef struct udp_client ev_udp_client;

/*
 * Core actions of a ev_server, callbacks to be executed at each of these
 * events happening
 */

/*
 * On new connection callback, defines the behaviour of the application when a
 * new client connects
 */
typedef void (*conn_callback)(ev_tcp_server *);

/*
 * On data incoming from an already connected client callback, defines what the
 * application must do with the stream of bytes received by a connected client
 */
typedef void (*recv_callback)(ev_tcp_client *);

/*
 * On write callback, once the application receives and process a bunch of
 * bytes, this defines what and how to do with the response to be sent out
 */
typedef void (*send_callback)(ev_tcp_client *);

/*
 * Server abstraction, as of now it's pretty self-explanatory, it is composed
 * of the file descriptor for the listening socket, the backlog to be set on
 * listen system call, host and port to listen on, a pointer to the context
 * (must be set) and 3 main callbacks:
 * - on_connection: Will be triggere when a client contact the server just before
 *                  accepting a connection
 * - on_recv:       Generally set inside on_connection callback, define how to
 *                  react to incoming data from an alredy connected client
 * - on_send:       Optionally used as responses can be sent directly from
 *                  on_recv callback through `ev_tcp_write` call, define the
 *                  behaviour of the server on response to clients
 */
struct ev_server {
    int sfd;
#if defined(EPOLL) || defined(__linux__)
    int run;
#else
    int run[2];
#endif
    int backlog;
    int port;
    char host[0xff];
    ev_context *ctx;
    conn_callback on_connection;
    recv_callback on_recv;
    send_callback on_send;
};

/*
 * Client structure, aside from the obvious members, it carries a server
 * pointer and an opaque `ptr` for storing arbitrary data, if needed, by the
 * user
 */
struct tcp_client {
    int fd;
    size_t bufsize;
    size_t capacity;
    void *ptr;
    char *buf;
    ev_tcp_server *server;
};

/*
 * UDP client structure, conceptually it's equivalent to the `tcp_client` one
 * but it must store also a `struct sockaddr` for the sender/destinatary address
 */
struct udp_client {
    int fd;
    size_t bufsize;
    size_t capacity;
    struct sockaddr addr;
    void *ptr;
    char *buf;
    ev_udp_server *server;
};

/*
 * Sets the tcp backlog and the ev_context reference to an ev_tcp_server,
 * setting to NULL the 3 main actions callbacks.
 * The ev_context have to be alredy initialized or it returns an error.
 * Up to the caller to decide how to create the ev_tcp_server and thus manage,
 * its ownership and memory lifetime by allocating it on the heap or the
 * stack
 */
int ev_tcp_server_init(ev_tcp_server *, ev_context *, int);

/*
 * Make the tcp server in listening mode, requires an on_connection callback to
 * be defined and passed as argument or it will return an error.
 * Under the hood the listening socket created is set to non-blocking mode and
 * registered to the ev_tcp_server context as an EV_READ event with
 * `conn_callback` as a read-callback to be called on reading-ready event by
 * the kernel.
 */
int ev_tcp_server_listen(ev_tcp_server *, const char *, int, conn_callback);

/*
 * Stops a listening ev_tcp_server by removing it's listening socket from the
 * underlying running loop and closing it, finally it stops the underlying
 * eventloop
 */
void ev_tcp_server_stop(ev_tcp_server *);

/*
 * Start the tcp server, it's a blocking call that calls ev_run on the
 * underlyng ev_context
 */
void ev_tcp_server_run(ev_tcp_server *);

/*
 * Accept the connection, requires a pointer to ev_tcp_client and a on_recv
 * callback, othersiwse it will return an err. Up to the user to manage the
 * ownership of the client, tough generally it's advisable to allocate it on
 * the heap to being able to juggle it around other callbacks
 */
int ev_tcp_server_accept(ev_tcp_server *, ev_tcp_client *, recv_callback);

/*
 * Fires a EV_WRITE event using a service private function to just write the
 * content of the buffer to the client, return an error if no on_send callback
 * was set, see `ev_tcp_server_set_on_send`
 */
int ev_tcp_server_enqueue_write(ev_tcp_client *);

/*
 * Sets an on_recv callback to be called with new incoming data arriving from
 * the client
 */
void ev_tcp_server_set_on_recv(ev_tcp_server *, recv_callback);

/*
 * Sets an on_send callback to be called with new outcoming data to be sent to
 * the client
 */
void ev_tcp_server_set_on_send(ev_tcp_server *, send_callback);

/*
 * Read all the incoming bytes on the connected client FD and store the to the
 * client buffer along with the total size read.
 *
 * TODO
 * It's a non-blocking read so must check for EAGAIN errors on partial reads
 */
ssize_t ev_tcp_read(ev_tcp_client *);

/*
 * Read the defined number of bytes in order to fill the buffer of n bytes
 */
ssize_t ev_tcp_read_bytes(ev_tcp_client *, size_t);

/*
 * Write the content of the client buffer to the connected client FD and reset
 * the client buffer length to according to the numeber of bytes sent out.
 *
 * TODO
 * It's a non-blocking write so must check for EAGAIN errors on partia writes
 */
ssize_t ev_tcp_write(ev_tcp_client *);

/*
 * Close a connection by removing the client FD from the underlying ev_context
 * and closing it, free all resources allocated
 */
void ev_tcp_close_connection(ev_tcp_client *);

/*
 * Just a simple helper function to retrieve a text explanation of the common
 * errors returned by the helper APIs
 */
const char *ev_tcp_err(int);

/* Set non-blocking socket */
static inline int set_nonblocking(int fd) {
    int flags, result;
    flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
        goto err;

    result = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (result == -1)
        goto err;

    return EV_OK;

err:

    fprintf(stderr, "set_nonblocking: %s\n", strerror(errno));
    return EV_ERR;
}

/*
 * ===================================================================
 *  Service private callbacks, acts as a bridge for scheudling server
 *  callbacks
 * ===================================================================
 */

static void on_accept(ev_context *ctx, void *data) {
    (void) ctx;
    ev_tcp_server *server = data;
    server->on_connection(server);
}

static void on_recv(ev_context *ctx, void *data) {
    (void) ctx;
    ev_tcp_client *client = data;
    client->server->on_recv(client);
}

static void on_send(ev_context *ctx, void *data) {
    (void) ctx;
    ev_tcp_client *client = data;
    client->server->on_send(client);
}

static void on_stop(ev_context *ctx, void *data) {
    (void) ctx;
    (void) data;
    printf("Stop\n");
    ctx->stop = 1;
}

/*
 * =================
 *  APIs definition
 * =================
 */

int ev_tcp_server_init(ev_tcp_server *server, ev_context *ctx, int backlog) {
    if (!ctx)
        return EV_TCP_MISSING_CONTEXT;
    server->backlog = backlog;
    // TODO check for context running
    server->ctx = ctx;
#if defined(EPOLL) || defined(__linux__)
    server->run = eventfd(0, EFD_NONBLOCK);
    ev_register_event(server->ctx, server->run,
                      EV_CLOSEFD|EV_READ, on_stop, NULL);
#else
    pipe(server->run);
    ev_register_event(server->ctx, server->run[1],
                      EV_CLOSEFD|EV_READ, on_stop, NULL);
#endif
    server->on_connection = NULL;
    server->on_recv = NULL;
    server->on_send = NULL;
    return EV_OK;
}

void ev_tcp_server_set_on_recv(ev_tcp_server *server, recv_callback on_recv) {
    server->on_recv = on_recv;
}

void ev_tcp_server_set_on_send(ev_tcp_server *server, send_callback on_send) {
    server->on_send = on_send;
}

int ev_tcp_server_listen(ev_tcp_server *server, const char *host,
                         int port, conn_callback on_connection) {

    if (!on_connection)
        return EV_TCP_MISSING_CALLBACK;

    int listen_fd = -1;
    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE
    };
    struct addrinfo *result, *rp;
    char port_str[6];

    snprintf(port_str, 6, "%i", port);

    if (getaddrinfo(host, port_str, &hints, &result) != 0)
        goto err;

    /* Create a listening socket */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_fd < 0) continue;
        /* Bind it to the addr:port opened on the network interface */
        if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break; // Succesful bind
        close(listen_fd);
    }

    freeaddrinfo(result);
    if (rp == NULL)
        goto err;

    /*
     * Let's make the socket non-blocking (strongly advised to use the
     * eventloop)
     */
    (void) set_nonblocking(listen_fd);

    /* Finally let's make it listen */
    if (listen(listen_fd, server->backlog) != 0)
        goto err;

    server->sfd = listen_fd;
    snprintf(server->host, strlen(host), "%s", host);
    server->port = port;
    server->on_connection = on_connection;

    // Register to service callback
    ev_register_event(server->ctx, server->sfd, EV_READ, on_accept, server);

    return EV_TCP_SUCCESS;
err:
    return EV_TCP_FAILURE;
}

void ev_tcp_server_run(ev_tcp_server *server) {
    if (ev_is_running(server->ctx) == 1)
        return;
    // Blocking call
    ev_run(server->ctx);
}

int ev_tcp_server_accept(ev_tcp_server *server,
                         ev_tcp_client *client, recv_callback on_data) {
    if (!on_data)
        return EV_TCP_MISSING_CALLBACK;
    server->on_recv = on_data;
    while (1) {
        int fd;
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);

        /* Let's accept on listening socket */
        if ((fd = accept(server->sfd, (struct sockaddr *) &addr, &addrlen)) < 0)
            break;

        if (fd == 0)
            continue;

        /* Make the new accepted socket non-blocking */
        (void) set_nonblocking(fd);

        // XXX placeholder
        client->fd = fd;
        client->bufsize = 0;
        client->capacity = EV_TCP_BUFSIZE;
        client->buf = calloc(1, EV_TCP_BUFSIZE);
        client->ptr = NULL;
        client->server = server;

        int err = ev_register_event(server->ctx, fd, EV_READ, on_recv, client);
        if (err < 0)
            return EV_TCP_FAILURE;
    }
    return EV_TCP_SUCCESS;
}

int ev_tcp_server_enqueue_write(ev_tcp_client *client) {
    if (!client->server->on_send)
        return EV_TCP_MISSING_CALLBACK;
    int err = ev_fire_event(client->server->ctx,
                            client->fd, EV_WRITE, on_send, client);
    if (err < 0)
        return EV_TCP_FAILURE;
    return EV_TCP_SUCCESS;
}

ssize_t ev_tcp_read(ev_tcp_client *client) {
    ssize_t n = 0;
    /* Read incoming stream of bytes */
    do {
        n = read(client->fd, client->buf + client->bufsize,
                 client->capacity - client->bufsize);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
                return n;
        }
        client->bufsize += n;
        /* Re-size the buffer in case of overflow of bytes */
        if (client->bufsize == client->capacity) {
            client->capacity *= 2;
            client->buf = realloc(client->buf, client->capacity);
        }
    } while (n > 0);

    /* 0 bytes read means disconnection by the client */
    if (n == 0)
        ev_tcp_close_connection(client);

    return client->bufsize;
}

ssize_t ev_tcp_read_bytes(ev_tcp_client *client, size_t size) {
    ssize_t n = 0;
    /* Read incoming stream of bytes */
    do {
        n = read(client->fd, client->buf + client->bufsize,
                 size - client->bufsize);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
                return n;
        }
        client->bufsize += n;
        /* Re-size the buffer in case of overflow of bytes */
        if (client->bufsize == client->capacity) {
            client->capacity *= 2;
            client->buf = realloc(client->buf, client->capacity);
        }
    } while (n > 0);

    /*
     * If EAGAIN happened and there still more data to read, re-arm
     * for a read on the next loop cycle, hopefully the kernel will be
     * available to send remaining data
     */
    if (client->bufsize < size && (errno == EAGAIN || errno == EWOULDBLOCK))
        ev_fire_event(client->server->ctx, client->fd, EV_READ, on_recv, client);

    /* 0 bytes read means disconnection by the client */
    if (n == 0)
        ev_tcp_close_connection(client);

    return client->bufsize;
}

void ev_tcp_server_stop(ev_tcp_server *server) {
    ev_del_fd(server->ctx, server->sfd);
    close(server->sfd);
#if defined(EPOLL) || defined(__linux__)
    eventfd_write(server->run, 1);
#else
    (void) write(server->run[0], &(unsigned long){1}, sizeof(unsigned long));
#endif
}

ssize_t ev_tcp_write(ev_tcp_client *client) {
    ssize_t n = 0, wrote = 0;

    /* Let's reply to the client */
    while (client->bufsize > 0) {
        n = write(client->fd, client->buf + n, client->bufsize);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
                return n;
        }
        client->bufsize -= n;
        wrote += n;
    }

    if (!client->server->on_recv)
        return EV_TCP_MISSING_CALLBACK;

    /*
     * If EAGAIN happened and there still more data to be written out, re-arm
     * for a write on the next loop cycle, hopefully the kernel will be
     * available to send remaining data
     */
    if (client->bufsize > 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        ev_fire_event(client->server->ctx, client->fd, EV_WRITE, on_send, client);
        goto err;
    }

    /* Re-arm for read */
    int err = ev_fire_event(client->server->ctx,
                            client->fd, EV_READ, on_recv, client);
    if (err < 0)
        goto err;

    return wrote;

err:
    return EV_TCP_FAILURE;
}

void ev_tcp_close_connection(ev_tcp_client *client) {
    ev_del_fd(client->server->ctx, client->fd);
    close(client->fd);
    free(client->buf);
    free(client);
}

const char *ev_tcp_err(int rc) {
    switch (rc) {
        case EV_TCP_SUCCESS:
            return "Success";
        case EV_TCP_FAILURE:
            return "Failure";
        case EV_TCP_MISSING_CALLBACK:
            return "Missing callback";
        default:
            return "Unknown error";
    }
}

#endif
