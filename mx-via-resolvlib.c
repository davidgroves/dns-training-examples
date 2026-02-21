/*
 * Resolve MX records for a domain name given on the command line.
 * Uses the standard resolver library (libresolv) and res_nquery().
 *
 * Build: cc -o mx-via-resolvlib mx-via-resolvlib.c -lresolv
 */

#include <arpa/nameser.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    res_state state;
    u_char answer[NS_PACKETSZ];
    int len, i, n;
    ns_msg handle;
    ns_rr rr;
    char namebuf[NS_MAXDNAME];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <domain>\n", argv[0]);
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

    len = res_nquery(state, argv[1], ns_c_in, ns_t_mx, answer, sizeof(answer));
    res_nclose(state);
    free(state);

    if (len < 0) {
        fprintf(stderr, "res_nquery failed\n");
        return 1;
    }

    if (ns_initparse(answer, len, &handle) < 0) {
        fprintf(stderr, "ns_initparse failed\n");
        return 1;
    }

    n = ns_msg_count(handle, ns_s_an);
    if (n == 0) {
        printf("No MX records for %s\n", argv[1]);
        return 0;
    }

    printf("MX records for %s:\n", argv[1]);
    for (i = 0; i < n; i++) {
        if (ns_parserr(&handle, ns_s_an, i, &rr) < 0) {
            fprintf(stderr, "ns_parserr failed\n");
            return 1;
        }
        if (ns_rr_type(rr) != ns_t_mx)
            continue;
        const u_char *rdata = ns_rr_rdata(rr);
        uint16_t preference = ns_get16(rdata);
        if (ns_name_uncompress(answer, answer + len, rdata + 2, namebuf, sizeof(namebuf)) < 0) {
            fprintf(stderr, "ns_name_uncompress failed\n");
            return 1;
        }
        printf("  %5u  %s\n", preference, namebuf);
    }

    return 0;
}
