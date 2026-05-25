/*
 * golden_driver.c -- Golden master for mpfr_const_euler_bs_init.
 *
 * The C function is `static`; we re-implement (golden-driver-substitute
 * pattern per ADR 0002): emit a fresh 6-tuple of zero bigints.
 *
 * Wire: {"inputs":{},"output":{"P":"0","Q":"0","T":"0","C":"0","D":"0","V":"0"}}.
 *
 * Tag distribution: no-arg single-output function; single happy case
 * per the buildopt_float128_p carve-out.
 */
#include "common.h"
#include <inttypes.h>

int main(void) {
    FILE *out = stdout;
    const uint64_t elapsed = 0;
    jl_begin(out, "happy");
    jl_end_inputs(out);
    fputs(",\"output\":{\"P\":\"0\",\"Q\":\"0\",\"T\":\"0\",\"C\":\"0\",\"D\":\"0\",\"V\":\"0\"}", out);
    jl_finish(out, elapsed);
    return 0;
}
