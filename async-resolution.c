/*
 * Resolve a name N times in parallel using c-ares.
 * Same CLI as sync-resolution; prints total program time at the end.
 *
 * Usage: async-resolution <name> <type> <N> [server]
 *        server: optional DNS server (IPv4, IPv6, or CSV); uses system default if omitted.
 *
 * Build: cc -o async-resolution async-resolution.c -lcares
 *        (requires libc-ares-dev or equivalent)
 */

#include <arpa/nameser.h>
#include <ares.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <time.h>

#define MAX_FDS 1024

static int record_type_from_string(const char *s) {
    static const struct {
        const char *name;
        int type;
    } types[] = {
        { "A", ns_t_a },
        { "AAAA", ns_t_aaaa },
        { "CNAME", ns_t_cname },
        { "MX", ns_t_mx },
        { "NS", ns_t_ns },
        { "PTR", ns_t_ptr },
        { "SOA", ns_t_soa },
        { "SRV", ns_t_srv },
        { "TXT", ns_t_txt },
        { "ANY", ns_t_any },
    };
    for (size_t i = 0; i < sizeof types / sizeof types[0]; i++) {
        if (strcasecmp(types[i].name, s) == 0)
            return types[i].type;
    }
    return -1;
}

typedef struct {
    int n;
    int completed;
    int done;
    struct timespec end_time;
} query_ctx_t;

static void query_callback(void *arg, int status, int timeouts, unsigned char *abuf, int alen) {
    (void)status;
    (void)timeouts;
    (void)abuf;
    (void)alen;
    query_ctx_t *ctx = arg;
    ctx->completed++;
    if (ctx->completed >= ctx->n) {
        ctx->done = 1;
        clock_gettime(CLOCK_MONOTONIC, &ctx->end_time);
    }
}

int main(int argc, char **argv) {
    ares_channel_t *channel = NULL;
    struct ares_options options;
    int type, n, i, nfds, nevents;
    fd_set read_fds, write_fds;
    struct timeval tv, *tvp;
    struct timespec start;
    double elapsed;
    ares_fd_events_t events[MAX_FDS];
    query_ctx_t ctx = { .n = 0, .completed = 0, .done = 0 };
    const char *server;

    if (argc != 4 && argc != 5) {
        fprintf(stderr, "Usage: %s <name> <type> <N> [server]\n", argv[0]);
        return 1;
    }
    server = (argc == 5) ? argv[4] : NULL;

    type = record_type_from_string(argv[2]);
    if (type < 0) {
        fprintf(stderr, "Unknown record type: %s\n", argv[2]);
        return 1;
    }

    n = atoi(argv[3]);
    if (n <= 0) {
        fprintf(stderr, "N must be a positive integer\n");
        return 1;
    }
    ctx.n = n;

    if (ares_library_init(ARES_LIB_INIT_ALL) != ARES_SUCCESS) {
        fprintf(stderr, "ares_library_init failed\n");
        return 1;
    }

    memset(&options, 0, sizeof(options));
    if (ares_init_options(&channel, &options, 0) != ARES_SUCCESS) {
        fprintf(stderr, "ares_init_options failed\n");
        ares_library_cleanup();
        return 1;
    }

    if (server != NULL) {
        if (ares_set_servers_csv(channel, server) != ARES_SUCCESS) {
            fprintf(stderr, "Invalid server: %s\n", server);
            ares_destroy(channel);
            ares_library_cleanup();
            return 1;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        ares_destroy(channel);
        ares_library_cleanup();
        return 1;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    for (i = 0; i < n; i++) {
        ares_query(channel, argv[1], ns_c_in, type, query_callback, &ctx);
    }
#pragma GCC diagnostic pop

    while (!ctx.done) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        nfds = ares_fds(channel, &read_fds, &write_fds);
#pragma GCC diagnostic pop
        nevents = 0;
        if (nfds > 0 && nfds <= MAX_FDS) {
            for (i = 0; i < nfds; i++) {
                unsigned int ev = 0;
                if (FD_ISSET(i, &read_fds))
                    ev |= ARES_FD_EVENT_READ;
                if (FD_ISSET(i, &write_fds))
                    ev |= ARES_FD_EVENT_WRITE;
                if (ev) {
                    events[nevents].fd = (ares_socket_t)i;
                    events[nevents].events = ev;
                    nevents++;
                }
            }
        }
        if (nfds == 0) {
            tvp = ares_timeout(channel, NULL, &tv);
            if (tvp && (tvp->tv_sec > 0 || tvp->tv_usec > 0))
                select(0, NULL, NULL, NULL, tvp);
            ares_process_fds(channel, NULL, 0, 0);
            continue;
        }
        tvp = ares_timeout(channel, NULL, &tv);
        if (tvp && (tvp->tv_sec > 0 || tvp->tv_usec > 0)) {
            if (select(nfds, &read_fds, &write_fds, NULL, tvp) < 0) {
                perror("select");
                ares_destroy(channel);
                ares_library_cleanup();
                return 1;
            }
        } else {
            if (select(nfds, &read_fds, &write_fds, NULL, NULL) < 0) {
                perror("select");
                ares_destroy(channel);
                ares_library_cleanup();
                return 1;
            }
        }
        nevents = 0;
        for (i = 0; i < nfds; i++) {
            unsigned int ev = 0;
            if (FD_ISSET(i, &read_fds))
                ev |= ARES_FD_EVENT_READ;
            if (FD_ISSET(i, &write_fds))
                ev |= ARES_FD_EVENT_WRITE;
            if (ev) {
                events[nevents].fd = (ares_socket_t)i;
                events[nevents].events = ev;
                nevents++;
            }
        }
        ares_process_fds(channel, events, (size_t)nevents, 0);
    }

    ares_destroy(channel);
    ares_library_cleanup();

    elapsed = (ctx.end_time.tv_sec - start.tv_sec) + (ctx.end_time.tv_nsec - start.tv_nsec) / 1e9;
    printf("Total time: %.6f s (%d resolutions)\n", elapsed, n);

    return 0;
}
