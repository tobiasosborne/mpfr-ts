/*
 * golden_driver.c -- Golden master for MPFR's mpfr_custom_init.
 *
 * C body is `return;` (no-op). The TS port returns posZero(prec) per
 * the mpfr_init2 convention. The driver builds a posZero(prec) MPFR
 * with libmpfr and emits it as the expected output.
 *
 * Ref: mpfr/src/stack_interface.c L32-L37.
 * Wire: {"inputs":{"prec":"<dec>"},"output":<mpfr posZero>}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

static inline void emit_case(FILE *out, const char *tag, uint64_t prec) {
    assert(prec >= 1);
    /* The TS port returns posZero(prec): {kind:'zero', sign:+1,
     * prec:<p>, exp:0, mant:0}. Build the equivalent libmpfr value to
     * emit on the wire. */
    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_zero(x, +1);
    const uint64_t t0 = now_ns();
    /* Call into the C function for fidelity (it does nothing on x;
     * but we want to measure the call time and have it in the trace). */
    /* mpfr_custom_init needs a void*; we pass a dummy. */
    char dummy[8];
    mpfr_custom_init(dummy, (mpfr_prec_t)prec);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "prec", prec);
    jl_end_inputs(out);
    jl_output_mpfr(out, x);
    jl_finish(out, elapsed);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    /* happy: 20 -- common precs. */
    emit_case(out, "happy", 53);
    emit_case(out, "happy", 24);
    emit_case(out, "happy", 64);
    emit_case(out, "happy", 113);
    emit_case(out, "happy", 100);
    emit_case(out, "happy", 200);
    emit_case(out, "happy", 256);
    emit_case(out, "happy", 512);
    emit_case(out, "happy", 1024);
    emit_case(out, "happy", 32);
    emit_case(out, "happy", 8);
    emit_case(out, "happy", 16);
    emit_case(out, "happy", 4);
    emit_case(out, "happy", 2);
    emit_case(out, "happy", 96);
    emit_case(out, "happy", 192);
    emit_case(out, "happy", 384);
    emit_case(out, "happy", 768);
    emit_case(out, "happy", 1536);
    emit_case(out, "happy", 2048);
    /* edge: 30 -- prec=1, limb boundaries, large precs. */
    emit_case(out, "edge", 1);
    emit_case(out, "edge", 3);
    emit_case(out, "edge", 63);
    emit_case(out, "edge", 64);
    emit_case(out, "edge", 65);
    emit_case(out, "edge", 127);
    emit_case(out, "edge", 128);
    emit_case(out, "edge", 129);
    emit_case(out, "edge", 191);
    emit_case(out, "edge", 192);
    emit_case(out, "edge", 193);
    emit_case(out, "edge", 255);
    emit_case(out, "edge", 257);
    emit_case(out, "edge", 511);
    emit_case(out, "edge", 513);
    emit_case(out, "edge", 1023);
    emit_case(out, "edge", 1025);
    emit_case(out, "edge", 2047);
    emit_case(out, "edge", 4096);
    emit_case(out, "edge", 8192);
    emit_case(out, "edge", 16384);
    emit_case(out, "edge", 17);
    emit_case(out, "edge", 33);
    emit_case(out, "edge", 49);
    emit_case(out, "edge", 89);
    emit_case(out, "edge", 199);
    emit_case(out, "edge", 333);
    emit_case(out, "edge", 555);
    emit_case(out, "edge", 999);
    emit_case(out, "edge", 1001);
    /* adversarial: 12 -- very small and very large precs. */
    emit_case(out, "adversarial", 1);
    emit_case(out, "adversarial", 2);
    emit_case(out, "adversarial", 3);
    emit_case(out, "adversarial", 7);
    emit_case(out, "adversarial", 11);
    emit_case(out, "adversarial", 23);
    emit_case(out, "adversarial", 50000);
    emit_case(out, "adversarial", 100000);
    emit_case(out, "adversarial", 500000);
    emit_case(out, "adversarial", 1000000);
    emit_case(out, "adversarial", 2000000);
    emit_case(out, "adversarial", 4000000);
    /* fuzz: 50 -- PRNG. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xACEDEFAB0BBADBABULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 5000);
            emit_case(out, "fuzz", prec);
        }
    }
    /* mined: 5 */
    emit_case(out, "mined", 53);
    emit_case(out, "mined", 100);
    emit_case(out, "mined", 64);
    emit_case(out, "mined", 200);
    emit_case(out, "mined", 1);
    return 0;
}
