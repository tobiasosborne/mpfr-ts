/*
 * golden_driver.c — Golden master for MPFR's mpfr_round_p.
 *
 * C signature
 * -----------
 *
 *   int mpfr_round_p(mp_limb_t *bp, mp_size_t bn, mpfr_exp_t err0,
 *                    mpfr_prec_t prec);
 *
 * Returns non-zero iff bp (as a bn-limb little-endian raw mantissa)
 * with `err0` known bits and target precision `prec` can be rounded
 * toward zero unambiguously. Precondition: bp[bn-1] & MPFR_LIMB_HIGHBIT.
 *
 * Ref: mpfr/src/round_p.c L67-L135.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"bp":["<dec>","<dec>",...],"err0":"<decimal>","prec":"<decimal>"},
 *    "output":true|false,
 *    "time_ns":<n>}
 *
 *   - bp via jl_kv_limbs — array of decimal-string limbs, little-endian.
 *   - err0 and prec via jl_kv_i64 (err0 signed; prec positive).
 *   - output via jl_output_scalar_bool — bare JSON boolean.
 *
 * Tag distribution: 22/30/10/55/5.
 *
 * Ref: src/ops/round_p.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_round_p golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* mpfr_round_p is __MPFR_DECLSPEC in mpfr-impl.h — not in the public
 * mpfr.h. Forward-declare for the public-header-only build. */
extern int mpfr_round_p (mp_limb_t *, mp_size_t, mpfr_exp_t, mpfr_prec_t);

/* Helper: emit one case. The C function may mutate bp internally? Per
 * the source it only reads; we pass a non-const pointer because the
 * declaration says so. We pass a copy to be safe. */
static inline void emit_case(FILE *out, const char *tag,
                             const mp_limb_t *bp_in, mp_size_t bn,
                             mpfr_exp_t err0, mpfr_prec_t prec) {
    assert(bn >= 1);
    /* HIGHBIT is defined below. Forward-use is fine since we're inside
     * a function declared before the macro. Actually use raw bit-shift. */
    assert(bp_in[bn - 1] & ((mp_limb_t)1 << (GMP_NUMB_BITS - 1)));
    assert(prec >= 1);

    mp_limb_t *bp = malloc(sizeof(mp_limb_t) * (size_t)bn);
    memcpy(bp, bp_in, sizeof(mp_limb_t) * (size_t)bn);

    const uint64_t t0 = now_ns();
    const int rc = mpfr_round_p(bp, bn, err0, prec);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_limbs(out, 1, "bp", bp_in, (size_t)bn);
    jl_kv_i64(out, 0, "err0", (int64_t)err0);
    jl_kv_i64(out, 0, "prec", (int64_t)prec);
    jl_end_inputs(out);
    jl_output_scalar_bool(out, rc);
    jl_finish(out, elapsed);

    free(bp);
}

/* Convenience: emit with a single-limb top-bit-set bp. */
static inline void emit_1limb(FILE *out, const char *tag, mp_limb_t v,
                              mpfr_exp_t err0, mpfr_prec_t prec) {
    mp_limb_t bp[1] = { v };
    emit_case(out, tag, bp, 1, err0, prec);
}

/* MSB-set bit pattern: 0xC000... (top two bits set). */
#define HIGHBIT  ((mp_limb_t)1 << (GMP_NUMB_BITS - 1))

int main(void) {
    FILE *out = stdout;

    /* happy: 22 — typical cases with MSB-set single limb. */
    /* MSB-set mantissa, plenty of error margin. */
    emit_1limb(out, "happy", HIGHBIT | (mp_limb_t)0x42, 60, 30);
    emit_1limb(out, "happy", HIGHBIT | (mp_limb_t)0x123, 60, 30);
    emit_1limb(out, "happy", HIGHBIT | (mp_limb_t)0x12345, 50, 24);
    emit_1limb(out, "happy", HIGHBIT | (mp_limb_t)0xABCDEF, 60, 24);
    emit_1limb(out, "happy", HIGHBIT, 60, 24);                 /* exact MSB only */
    emit_1limb(out, "happy", HIGHBIT | (mp_limb_t)0xFF, 60, 53);
    emit_1limb(out, "happy", HIGHBIT | (mp_limb_t)1, 32, 24);
    emit_1limb(out, "happy", HIGHBIT | (HIGHBIT >> 1), 64, 53);  /* 0xC0... */
    emit_1limb(out, "happy", HIGHBIT | (HIGHBIT >> 30), 60, 24);
    emit_1limb(out, "happy", ~(mp_limb_t)0, 64, 30);          /* all ones */
    emit_1limb(out, "happy", HIGHBIT | ((mp_limb_t)1 << 40), 50, 24);
    emit_1limb(out, "happy", HIGHBIT | (mp_limb_t)0x7F, 50, 24);
    emit_1limb(out, "happy", HIGHBIT | (mp_limb_t)0x80, 50, 24);
    emit_1limb(out, "happy", HIGHBIT | (mp_limb_t)0x100, 50, 30);
    emit_1limb(out, "happy", HIGHBIT | (mp_limb_t)0x200, 50, 30);
    emit_1limb(out, "happy", HIGHBIT | (mp_limb_t)0xFF00, 60, 40);
    /* Multi-limb: bn=2 with MSB-set top. */
    { mp_limb_t bp[2] = { 0x123456789ABCDEFULL, HIGHBIT | 0x42 }; emit_case(out, "happy", bp, 2, 120, 60); }
    { mp_limb_t bp[2] = { 0xFFFFFFFFFFFFFFFFULL, HIGHBIT }; emit_case(out, "happy", bp, 2, 100, 50); }
    { mp_limb_t bp[2] = { 0, HIGHBIT | 0x1 }; emit_case(out, "happy", bp, 2, 100, 50); }
    /* bn=3. */
    { mp_limb_t bp[3] = { 0x123, 0x456, HIGHBIT | 0x789 }; emit_case(out, "happy", bp, 3, 180, 100); }
    { mp_limb_t bp[3] = { 0, 0, HIGHBIT }; emit_case(out, "happy", bp, 3, 180, 100); }
    /* Tight precision near total bits. */
    emit_1limb(out, "happy", HIGHBIT | (mp_limb_t)0x42, 64, 50);

    /* edge: 30 — boundary conditions: err0 == prec, err0 <= 0, prec near bn*bits. */
    /* err0 <= 0 → return 0. */
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, 0, 30);
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, -10, 30);
    /* err0 == prec → return 0 (uexp_t cast in C; we test the >= case). */
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, 30, 30);
    /* err0 < prec → return 0. */
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, 24, 30);
    /* prec >= bn*bits → return 0. */
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, 60, 64);
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, 60, 65);
    /* err0 = prec + 1. */
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, 31, 30);
    /* prec = 1. */
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, 60, 1);
    emit_1limb(out, "edge", HIGHBIT, 60, 1);
    /* prec = bn*bits - 1. */
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, 64, 63);
    /* err0 in the same limb as prec. */
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x12345, 40, 30);
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x12345, 35, 30);
    /* err0 spans different limb (multi-limb). */
    { mp_limb_t bp[2] = { 0x100, HIGHBIT | 0x1 }; emit_case(out, "edge", bp, 2, 100, 60); }
    { mp_limb_t bp[2] = { 0, HIGHBIT }; emit_case(out, "edge", bp, 2, 100, 50); }
    { mp_limb_t bp[2] = { ~(mp_limb_t)0, HIGHBIT }; emit_case(out, "edge", bp, 2, 100, 50); }
    /* All-zero middle limbs. */
    { mp_limb_t bp[3] = { 1, 0, HIGHBIT }; emit_case(out, "edge", bp, 3, 180, 80); }
    { mp_limb_t bp[3] = { 0, 0, HIGHBIT | 1 }; emit_case(out, "edge", bp, 3, 180, 100); }
    /* All-one middle limbs. */
    { mp_limb_t bp[3] = { ~(mp_limb_t)0, ~(mp_limb_t)0, HIGHBIT }; emit_case(out, "edge", bp, 3, 180, 80); }
    { mp_limb_t bp[3] = { ~(mp_limb_t)0, ~(mp_limb_t)0, HIGHBIT | (HIGHBIT >> 1) }; emit_case(out, "edge", bp, 3, 180, 80); }
    /* prec = 0 is invalid per assertion; skip. */
    /* err0 just past prec by 1. */
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, 25, 24);
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, 32, 24);
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, 53, 50);
    /* Limb-boundary err0. */
    emit_1limb(out, "edge", HIGHBIT | (mp_limb_t)0x42, 64, 32);
    /* MSB-only mantissa. */
    emit_1limb(out, "edge", HIGHBIT, 32, 24);
    emit_1limb(out, "edge", HIGHBIT, 50, 24);
    /* Multi-limb large prec. */
    { mp_limb_t bp[4] = { 1, 2, 3, HIGHBIT | 4 }; emit_case(out, "edge", bp, 4, 240, 200); }
    { mp_limb_t bp[4] = { 0, 0, 0, HIGHBIT }; emit_case(out, "edge", bp, 4, 240, 200); }
    { mp_limb_t bp[4] = { ~(mp_limb_t)0, ~(mp_limb_t)0, ~(mp_limb_t)0, HIGHBIT }; emit_case(out, "edge", bp, 4, 240, 200); }
    { mp_limb_t bp[4] = { 1, 0, 0, HIGHBIT | 1 }; emit_case(out, "edge", bp, 4, 240, 100); }
    { mp_limb_t bp[4] = { 0, 1, 0, HIGHBIT }; emit_case(out, "edge", bp, 4, 240, 100); }

    /* adversarial: 10 — patterns that catch off-by-one errors. */
    /* All-zero error band: bit pattern just inside prec. */
    emit_1limb(out, "adversarial", HIGHBIT | (mp_limb_t)1, 40, 30);
    emit_1limb(out, "adversarial", HIGHBIT | ((mp_limb_t)1 << 30), 40, 30);
    /* All-one error band variant. */
    emit_1limb(out, "adversarial", HIGHBIT | ((mp_limb_t)0xFF << 25), 40, 30);
    /* Tight prec close to err0. */
    emit_1limb(out, "adversarial", HIGHBIT | (mp_limb_t)0xAA, 31, 30);
    /* Multi-limb cancellation-style: 0xFF...FF in low, HIGHBIT in top. */
    { mp_limb_t bp[2] = { ~(mp_limb_t)0, HIGHBIT | 0x1 }; emit_case(out, "adversarial", bp, 2, 100, 60); }
    /* 0x80...01 in low. */
    { mp_limb_t bp[2] = { ((mp_limb_t)1 << 63) | 1, HIGHBIT }; emit_case(out, "adversarial", bp, 2, 100, 50); }
    /* Multi-limb with zeros after high. */
    { mp_limb_t bp[3] = { 1, 0, HIGHBIT | 0x12345 }; emit_case(out, "adversarial", bp, 3, 180, 80); }
    /* Mixed pattern. */
    { mp_limb_t bp[3] = { 0xAAAAAAAAAAAAAAAAULL, 0x5555555555555555ULL, HIGHBIT | 0xFF }; emit_case(out, "adversarial", bp, 3, 180, 100); }
    /* MSB only with very tight prec. */
    emit_1limb(out, "adversarial", HIGHBIT, 64, 63);
    /* prec at the limb boundary. */
    emit_1limb(out, "adversarial", HIGHBIT | (mp_limb_t)0x42, 64, 64);  /* prec == bn*bits → 0 */

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x1234567890ABCDEFULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t bn = 1 + xs64_below(&rng, 4);  /* 1..4 limbs */
            mp_limb_t bp[4];
            for (uint64_t i = 0; i < bn; ++i) bp[i] = (mp_limb_t)xs64_next(&rng);
            bp[bn - 1] |= HIGHBIT;  /* enforce MSB-set */
            const uint64_t total_bits = bn * GMP_NUMB_BITS;
            const uint64_t prec = 1 + xs64_below(&rng, total_bits - 1);
            /* err0 in a range that hits both yes and no branches:
             * sometimes err0 > prec (could-round), sometimes err0 <= prec (can't). */
            const int64_t err0 = (int64_t)(xs64_below(&rng, total_bits + 30) + 1) - 5;
            emit_case(out, "fuzz", bp, (mp_size_t)bn, (mpfr_exp_t)err0, (mpfr_prec_t)prec);
        }
    }

    /* mined: 5 — patterns from mpfr/tests/troundp.c. */
    /* mpfr/tests/troundp.c uses simple values mostly testing 0/non-0. */
    emit_1limb(out, "mined", HIGHBIT | (mp_limb_t)0x42, 60, 30);
    emit_1limb(out, "mined", HIGHBIT, 50, 24);
    emit_1limb(out, "mined", HIGHBIT | (mp_limb_t)0xAA, 40, 24);
    { mp_limb_t bp[2] = { 0, HIGHBIT }; emit_case(out, "mined", bp, 2, 100, 53); }
    { mp_limb_t bp[2] = { 0x123, HIGHBIT | 0x456 }; emit_case(out, "mined", bp, 2, 120, 53); }

    return 0;
}
