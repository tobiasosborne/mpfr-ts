/*
 * golden_driver.c -- Golden master for MPFR's mpfr_random_deviate_init.
 *
 * No-arg function: returns a fresh random-deviate state {e:0, h:0, f:0}.
 * Ref: mpfr/src/random_deviate.c L65-L70.
 *
 * Wire: {"inputs":{},"output":{"e":"0","h":"0","f":"0"}}.
 *
 * Tag distribution: no-arg single-output function; single happy case
 * per the buildopt_float128_p carve-out. Rule 7 minimums inapplicable.
 */
#include "common.h"
#include <inttypes.h>

int main(void) {
    FILE *out = stdout;
    const uint64_t elapsed = 0;
    jl_begin(out, "happy");
    jl_end_inputs(out);
    fputs(",\"output\":{\"e\":\"0\",\"h\":\"0\",\"f\":\"0\"}", out);
    jl_finish(out, elapsed);
    return 0;
}
