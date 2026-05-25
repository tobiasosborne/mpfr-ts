/*
 * golden_driver.c -- Golden master for MPFR's mpfr_get_cputime.
 *
 * The TS port returns 0 unconditionally (CPU time not portable in
 * pure-TS, and the C function is internal logging-only). The driver
 * emits 0 to match -- NOT mpfr_get_cputime() which would yield a
 * host-dependent observation.
 *
 * Ref: mpfr/src/logging.c L114-L132.
 */
#include "common.h"
#include <inttypes.h>

int main(void) {
    FILE *out = stdout;
    /* TS-expected value is 0; do not call the actual mpfr_get_cputime. */
    const uint64_t elapsed = 0;
    jl_begin(out, "happy");
    jl_end_inputs(out);
    fprintf(out, ",\"output\":0");
    jl_finish(out, elapsed);
    return 0;
}
