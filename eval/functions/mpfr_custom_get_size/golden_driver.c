/*
 * golden_driver.c -- Golden master for MPFR's mpfr_custom_get_size.
 *
 * C: size_t mpfr_custom_get_size(mpfr_prec_t prec);
 *    return MPFR_PREC2LIMBS(prec) * MPFR_BYTES_PER_MP_LIMB;
 * Ref: mpfr/src/stack_interface.c L25-L30.
 *
 * On the target host (GMP_NUMB_BITS=64, 8 bytes per limb):
 *   bytes = ((prec + 63) / 64) * 8
 *
 * Wire: {"inputs":{"prec":"<dec>"},"output":"<dec>"}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_custom_get_size golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline void emit_case(FILE *out, const char *tag, uint64_t prec) {
    assert(prec >= 1);
    const uint64_t t0 = now_ns();
    const size_t bytes = mpfr_custom_get_size((mpfr_prec_t)prec);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "prec", prec);
    jl_end_inputs(out);
    jl_output_scalar_u64(out, (uint64_t)bytes);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;
    /* happy: 20 -- common IEEE precisions and round numbers. */
    emit_case(out, "happy", 53);    /* IEEE double */
    emit_case(out, "happy", 24);    /* IEEE float */
    emit_case(out, "happy", 64);    /* x87 extended */
    emit_case(out, "happy", 113);   /* IEEE binary128 */
    emit_case(out, "happy", 100);
    emit_case(out, "happy", 200);
    emit_case(out, "happy", 256);
    emit_case(out, "happy", 512);
    emit_case(out, "happy", 1024);
    emit_case(out, "happy", 2048);
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
    /* edge: 30 -- limb boundaries (63/64/65, 127/128/129, etc.) and PREC_MIN/MAX. */
    emit_case(out, "edge", 1);       /* PREC_MIN */
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
    emit_case(out, "edge", 256);
    emit_case(out, "edge", 257);
    emit_case(out, "edge", 511);
    emit_case(out, "edge", 512);
    emit_case(out, "edge", 513);
    emit_case(out, "edge", 1023);
    emit_case(out, "edge", 1025);
    emit_case(out, "edge", 4095);
    emit_case(out, "edge", 4096);
    emit_case(out, "edge", 4097);
    emit_case(out, "edge", 8191);
    emit_case(out, "edge", 8192);
    emit_case(out, "edge", 65535);
    emit_case(out, "edge", 65536);
    emit_case(out, "edge", 65537);
    emit_case(out, "edge", 1000000);
    emit_case(out, "edge", 1000063);
    emit_case(out, "edge", 1000064);
    emit_case(out, "edge", 1000065);
    /* adversarial: 12 -- prec just above multiples of 64; high but safe values. */
    emit_case(out, "adversarial", 2);
    emit_case(out, "adversarial", 3);
    emit_case(out, "adversarial", 7);
    emit_case(out, "adversarial", 11);
    emit_case(out, "adversarial", 17);
    emit_case(out, "adversarial", 31);
    emit_case(out, "adversarial", 33);
    emit_case(out, "adversarial", 89);
    emit_case(out, "adversarial", 123);
    emit_case(out, "adversarial", 199);
    emit_case(out, "adversarial", 333);
    emit_case(out, "adversarial", 4099);
    /* fuzz: 50 -- PRNG-driven prec in [1, 10000]. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xABABCDCD11220033ULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 10000);
            emit_case(out, "fuzz", prec);
        }
    }
    /* mined: 5 -- transcribed shapes from mpfr/tests/tcustom.c which
     * exercises mpfr_custom_get_size at various precisions. */
    emit_case(out, "mined", 53);    /* tcustom.c default */
    emit_case(out, "mined", 100);
    emit_case(out, "mined", 1);
    emit_case(out, "mined", 64);
    emit_case(out, "mined", 256);
    return 0;
}
