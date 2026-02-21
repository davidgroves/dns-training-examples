/*
 * Synchronously resolve a name N times using libresolv and res_nquery().
 * Prints total program time at the end.
 *
 * Usage: sync-resolution <name> <type> <N> [server]
 *        server: optional IPv4 address (e.g. 8.8.8.8); uses system default if omitted.
 *
 * Build: cc -o sync-resolution sync-resolution.c -lresolv
 */

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

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

int main(int argc, char **argv) {
    res_state state;
    u_char answer[NS_PACKETSZ];
    int len, n, type, i;
    struct timespec start, end;
    double elapsed;
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

    state = calloc(1, sizeof(*state));
    if (!state) {
        perror("calloc");
        return 1;
    }

    if (res_ninit(state) != 0) {
        fprintf(stderr, "res_ninit failed\n");
        free(state);
        return 1;
    }

    if (server != NULL) {
        struct in_addr addr;
        if (inet_pton(AF_INET, server, &addr) != 1) {
            fprintf(stderr, "Invalid server address (IPv4 only): %s\n", server);
            res_nclose(state);
            free(state);
            return 1;
        }
        state->nscount = 1;
        state->nsaddr_list[0].sin_family = AF_INET;
        state->nsaddr_list[0].sin_port = htons(53);
        state->nsaddr_list[0].sin_addr = addr;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        res_nclose(state);
        free(state);
        return 1;
    }

    for (i = 0; i < n; i++) {
        len = res_nquery(state, argv[1], ns_c_in, type, answer, sizeof(answer));
        if (len < 0) {
            fprintf(stderr, "res_nquery failed on iteration %d\n", i + 1);
            res_nclose(state);
            free(state);
            return 1;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("clock_gettime");
        res_nclose(state);
        free(state);
        return 1;
    }

    res_nclose(state);
    free(state);

    elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Total time: %.6f s (%d resolutions)\n", elapsed, n);

    return 0;
}
