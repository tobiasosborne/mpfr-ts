/*
 * golden_driver.c -- Golden master for MPFR's mpfr_get_emin_min.
 *
 * C: mpfr_exp_t mpfr_get_emin_min(void) -- return MPFR_EMIN_MIN.
 * Ref: mpfr/src/exceptions.c L52-L56.
 *
 * Constant accessor. Single happy case (Rule 7 carve-out).
 */
#include "common.h"
#include <inttypes.h>

int main(void) {
    FILE *out = stdout;
    const uint64_t t0 = now_ns();
    const long val = (long)mpfr_get_emin_min();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, "happy");
    jl_end_inputs(out);
    fprintf(out, ",\"output\":\"%ld\"", val);
    jl_finish(out, elapsed);
    return 0;
}
