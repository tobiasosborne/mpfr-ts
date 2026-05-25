/*
 * golden_driver.c -- Golden master for MPFR's mpfr_free_pool.
 *
 * C is void-returning; TS port is a no-op returning null.
 * The driver still calls mpfr_free_pool() to verify the C side runs
 * without error but emits JSON null as the expected output.
 *
 * Ref: mpfr/src/pool.c L105-L118.
 */
#include "common.h"
#include <inttypes.h>

int main(void) {
    FILE *out = stdout;
    const uint64_t t0 = now_ns();
    mpfr_free_pool();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, "happy");
    jl_end_inputs(out);
    jl_output_scalar_bool(out, 1);  /* true marker — see ref port */
    jl_finish(out, elapsed);
    return 0;
}
