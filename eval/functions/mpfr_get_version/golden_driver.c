/*
 * golden_driver.c -- Golden master for MPFR's mpfr_get_version.
 *
 * C: const char *mpfr_get_version(void) -- returns the upstream version
 *    string. Ref: mpfr/src/version.c L24-L28.
 *
 * Wire: {"inputs":{},"output":"<version-string>"}.
 *
 * Single happy case (no-arg accessor pattern). The version string comes
 * from the actual libmpfr at link time; the TS port mirrors the literal
 * value from the cloned mpfr/src/version.c source.
 */
#include "common.h"
#include <inttypes.h>

int main(void) {
    FILE *out = stdout;
    const uint64_t t0 = now_ns();
    const char *v = mpfr_get_version();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, "happy");
    jl_end_inputs(out);
    jl_output_scalar_str(out, v);
    jl_finish(out, elapsed);
    return 0;
}
