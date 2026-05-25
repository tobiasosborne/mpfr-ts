/*
 * golden_driver.c -- Golden master for MPFR's mpfr_get_emax_min.
 * Ref: mpfr/src/exceptions.c L88-L92.
 */
#include "common.h"
#include <inttypes.h>

int main(void) {
    FILE *out = stdout;
    const uint64_t t0 = now_ns();
    const long val = (long)mpfr_get_emax_min();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, "happy");
    jl_end_inputs(out);
    fprintf(out, ",\"output\":\"%ld\"", val);
    jl_finish(out, elapsed);
    return 0;
}
