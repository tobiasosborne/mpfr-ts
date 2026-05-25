/*
 * golden_driver.c -- Golden master for MPFR's mpfr_get_default_rounding_mode.
 * Ref: mpfr/src/set_rnd.c L34-L38.
 *
 * Single happy case; default-state read with no input.
 */
#include "common.h"
#include <inttypes.h>

int main(void) {
    FILE *out = stdout;
    /* Confirm default is RNDN at process start. */
    const uint64_t t0 = now_ns();
    const mpfr_rnd_t rnd = mpfr_get_default_rounding_mode();
    const uint64_t elapsed = now_ns() - t0;
    const char *rnd_str = (rnd == MPFR_RNDN) ? "RNDN"
                        : (rnd == MPFR_RNDZ) ? "RNDZ"
                        : (rnd == MPFR_RNDU) ? "RNDU"
                        : (rnd == MPFR_RNDD) ? "RNDD"
                        : (rnd == MPFR_RNDA) ? "RNDA"
                        : "UNKNOWN";
    jl_begin(out, "happy");
    jl_end_inputs(out);
    fprintf(out, ",\"output\":\"%s\"", rnd_str);
    jl_finish(out, elapsed);
    return 0;
}
