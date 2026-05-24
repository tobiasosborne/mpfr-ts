/*
 * golden_driver.c — Golden master for MPFR's mpfr_swap.
 *
 * C signature
 * -----------
 *
 *   void mpfr_swap(mpfr_ptr u, mpfr_ptr v);
 *
 * Swaps the prec/sign/exp/mantissa-pointer fields between u and v.
 * Ref: mpfr/src/swap.c L26-L53.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS: mpfr_swap(a: MPFR, b: MPFR) -> [MPFR, MPFR] returns [b, a]. To drive
 * the C side at parity:
 *
 *   1. Construct a, b (mpfr_t aa, bb).
 *   2. Call mpfr_swap(aa, bb).
 *   3. Emit output as an object with fields "0" (= aa post-swap, which is
 *      the original b) and "1" (= bb post-swap, which is the original a).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"a":<MPFR>,"b":<MPFR>},
 *    "output":{"0":<MPFR>,"1":<MPFR>},
 *    "time_ns":<n>}
 *
 * The TS port returns a tuple (an array); decodeExpectedOutput's
 * "object" branch handles this (the runner compares element-wise via
 * numeric-string keys "0", "1"). Generic-object decoding round-trips
 * MPFR fields via decodeInputValue → decodeMpfr.
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  22  (normal/zero/inf/nan combinations, common precs)
 *   edge         :  30  (PREC_MIN, precision mismatches, kind mismatches)
 *   adversarial  :  10  (NaN-with-NaN, signed zero permutations)
 *   fuzz         :  55  (PRNG random kinds, signs, precs)
 *   mined        :   5  (mpfr/tests/tswap.c patterns — there is no
 *                        tswap.c; we use generic swap-pair fixtures)
 *
 * Ref: mpfr/src/swap.c — C reference.
 * Ref: src/ops/swap.ts — production port (to be written by sonnet).
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

static const int SIGNS[2] = {+1, -1};

/* Emit ,"a":<mpfr>,"b":<mpfr> output object using jl_kv_mpfr internals.
 * We emit by hand because common.h doesn't have a "two-element MPFR object"
 * helper; the shape matches decodeExpectedOutput's "object" branch which
 * recurses on each key. The TS port returns { a, b } (an object with
 * named string keys), so we use string keys here too — the harness's
 * generic-object comparator iterates Object.entries() on the expected
 * fields and looks up actual[k]. */
static inline void
emit_swap_output(FILE *out, mpfr_srcptr first, mpfr_srcptr second) {
    fputs(",\"output\":{", out);
    jl_kv_mpfr(out, 1, "a", first);
    jl_kv_mpfr(out, 0, "b", second);
    fputs("}", out);
}

static void
emit_case(FILE *out, const char *tag, mpfr_srcptr a, mpfr_srcptr b) {
    /* Clone a and b into mutable temps for the swap. */
    mpfr_t aa, bb;
    mpfr_init2(aa, mpfr_get_prec(a));
    mpfr_init2(bb, mpfr_get_prec(b));
    mpfr_set(aa, a, MPFR_RNDN);
    mpfr_set(bb, b, MPFR_RNDN);

    const uint64_t t0 = now_ns();
    mpfr_swap(aa, bb);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "a", a);
    jl_kv_mpfr(out, 0, "b", b);
    jl_end_inputs(out);
    /* Post-swap: aa carries original b's bits, bb carries original a's
     * bits. TS output is [b, a] — index 0 is original b. */
    emit_swap_output(out, aa, bb);
    jl_finish(out, elapsed);

    mpfr_clear(aa);
    mpfr_clear(bb);
}

static void mk_norm_d(mpfr_ptr x, mpfr_prec_t prec, double d) {
    mpfr_init2(x, prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}

static void mk_norm_si(mpfr_ptr x, mpfr_prec_t prec, long v) {
    mpfr_init2(x, prec);
    mpfr_set_si(x, v, MPFR_RNDN);
}

static void mk_zero(mpfr_ptr x, mpfr_prec_t prec, int sign) {
    mpfr_init2(x, prec);
    mpfr_set_zero(x, sign);
}

static void mk_inf(mpfr_ptr x, mpfr_prec_t prec, int sign) {
    mpfr_init2(x, prec);
    mpfr_set_inf(x, sign);
}

static void mk_nan(mpfr_ptr x, mpfr_prec_t prec) {
    mpfr_init2(x, prec);
    mpfr_set_nan(x);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    {
        /* Normal + normal, same prec. */
        { mpfr_t a, b; mk_norm_d(a, 53, 3.14); mk_norm_d(b, 53, 2.71); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 53, -3.14); mk_norm_d(b, 53, 2.71); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 53, 3.14); mk_norm_d(b, 53, -2.71); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_si(a, 53, 7); mk_norm_si(b, 53, -7); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Normal + normal, different prec. */
        { mpfr_t a, b; mk_norm_d(a, 24, 1.5); mk_norm_d(b, 53, 3.14); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 113, 1.0/3.0); mk_norm_d(b, 53, 2.71); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 200, 1.5); mk_norm_d(b, 100, 3.14); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Zeros. */
        { mpfr_t a, b; mk_zero(a, 53, +1); mk_zero(b, 53, -1); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_zero(a, 53, +1); mk_norm_d(b, 53, 3.14); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_zero(a, 53, -1); mk_norm_d(b, 53, 3.14); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Infinities. */
        { mpfr_t a, b; mk_inf(a, 53, +1); mk_inf(b, 53, -1); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_inf(a, 53, +1); mk_norm_d(b, 53, 3.14); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_inf(a, 53, -1); mk_zero(b, 53, +1); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* NaN. */
        { mpfr_t a, b; mk_nan(a, 53); mk_norm_d(b, 53, 3.14); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_nan(a, 53); mk_zero(b, 53, +1); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_nan(a, 53); mk_inf(b, 53, -1); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Mix at common precs. */
        { mpfr_t a, b; mk_norm_d(a, 64, 1.5e10); mk_norm_d(b, 64, -1.5e-10); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_si(a, 113, 100); mk_norm_si(b, 113, -100); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 53, 0.5); mk_norm_d(b, 53, 0.25); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 53, 1e50); mk_norm_d(b, 53, 1e-50); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 100, 1.5); mk_norm_d(b, 200, 1.5); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 80, 1e30); mk_norm_d(b, 80, -1e-30); emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    /* edge: 30 */
    {
        /* PREC_MIN both sides. */
        { mpfr_t a, b; mk_norm_si(a, 1, 1); mk_norm_si(b, 1, -1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_si(a, 1, 1); mk_zero(b, 1, +1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_zero(a, 1, +1); mk_zero(b, 1, -1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_inf(a, 1, +1); mk_inf(b, 1, -1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_nan(a, 1); mk_nan(b, 1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* PREC_MIN one side, large other. */
        { mpfr_t a, b; mk_norm_si(a, 1, 1); mk_norm_d(b, 1024, 3.14); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 1024, 3.14); mk_norm_si(b, 1, -1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* All signed-zero permutations. */
        { mpfr_t a, b; mk_zero(a, 53, +1); mk_zero(b, 53, +1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_zero(a, 53, -1); mk_zero(b, 53, -1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_zero(a, 53, +1); mk_zero(b, 53, -1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_zero(a, 53, -1); mk_zero(b, 53, +1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* All inf permutations. */
        { mpfr_t a, b; mk_inf(a, 53, +1); mk_inf(b, 53, +1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_inf(a, 53, -1); mk_inf(b, 53, -1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_inf(a, 53, +1); mk_inf(b, 53, -1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Mixed sign normal. */
        { mpfr_t a, b; mk_norm_d(a, 53, -3.14); mk_norm_d(b, 53, -2.71); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Large prec mismatch. */
        { mpfr_t a, b; mk_norm_d(a, 24, 1.5); mk_norm_d(b, 1024, 1.5); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 1024, 1.5); mk_norm_d(b, 24, 1.5); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Limb-boundary precs (63/64/65, 127/128/129). */
        { mpfr_t a, b; mk_norm_d(a, 63, 1.5); mk_norm_d(b, 64, 1.5); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 64, 1.5); mk_norm_d(b, 65, 1.5); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 127, 1.5); mk_norm_d(b, 128, 1.5); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_norm_d(a, 128, 1.5); mk_norm_d(b, 129, 1.5); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Self-swap with same kind. */
        { mpfr_t a, b; mk_norm_d(a, 53, 3.14); mk_norm_d(b, 53, 3.14); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* NaN+inf, NaN+zero. */
        { mpfr_t a, b; mk_nan(a, 53); mk_inf(b, 53, +1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_nan(a, 53); mk_zero(b, 53, -1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_inf(a, 53, -1); mk_nan(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Extreme exponents. */
        { mpfr_t a, b; mk_norm_d(a, 53, 1e300); mk_norm_d(b, 53, 1e-300); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Asymmetric prec, mixed sign. */
        { mpfr_t a, b; mk_norm_si(a, 100, -3); mk_norm_si(b, 50, 5); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Inf vs zero. */
        { mpfr_t a, b; mk_inf(a, 53, +1); mk_zero(b, 53, +1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; mk_inf(a, 53, -1); mk_zero(b, 53, -1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Zero precs different. */
        { mpfr_t a, b; mk_zero(a, 24, +1); mk_zero(b, 113, -1); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    /* adversarial: 10 */
    {
        /* Swap-self-like: a and b structurally identical. */
        { mpfr_t a, b; mk_norm_d(a, 53, 1.5); mk_norm_d(b, 53, 1.5); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Asymmetric NaN inputs (both are TS-canonicalised). */
        { mpfr_t a, b; mk_nan(a, 53); mk_nan(b, 113); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* +0 vs -0 — sign preservation. */
        { mpfr_t a, b; mk_zero(a, 53, +1); mk_zero(b, 53, -1); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* +inf vs -inf — sign preservation. */
        { mpfr_t a, b; mk_inf(a, 53, +1); mk_inf(b, 53, -1); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Normal +/- with same magnitude. */
        { mpfr_t a, b; mk_norm_d(a, 53, 3.14); mk_norm_d(b, 53, -3.14); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Cross-kind: normal vs inf. */
        { mpfr_t a, b; mk_norm_d(a, 53, 1e10); mk_inf(b, 53, +1); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Cross-kind: zero vs normal. */
        { mpfr_t a, b; mk_zero(a, 53, -1); mk_norm_d(b, 53, -1.5); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* PREC_MIN normal vs MAX-ish normal — exercises full struct copy. */
        { mpfr_t a, b; mk_norm_si(a, 1, -1); mk_norm_d(b, 2048, 1.5); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Doubly-large precs. */
        { mpfr_t a, b; mk_norm_d(a, 4096, 1.5); mk_norm_d(b, 4096, -1.5); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* NaN both sides at distinct precs (canonicalisation test). */
        { mpfr_t a, b; mk_nan(a, 1); mk_nan(b, 2048); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5A1FACE000123456ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t pa = 1 + xs64_below(&rng, 200);
            const uint64_t pb = 1 + xs64_below(&rng, 200);
            const uint64_t ka = xs64_below(&rng, 10);
            const uint64_t kb = xs64_below(&rng, 10);
            mpfr_t a, b;
            mpfr_init2(a, (mpfr_prec_t)pa);
            mpfr_init2(b, (mpfr_prec_t)pb);
            /* Populate a. */
            if (ka < 7) {
                const double mag = (double)(1 + xs64_below(&rng, 1000000));
                const int sa = SIGNS[xs64_below(&rng, 2)];
                mpfr_set_d(a, sa * mag, MPFR_RNDN);
                if (!mpfr_regular_p(a)) { mpfr_clear(a); mpfr_clear(b); continue; }
            } else if (ka == 7) {
                mpfr_set_zero(a, SIGNS[xs64_below(&rng, 2)]);
            } else if (ka == 8) {
                mpfr_set_inf(a, SIGNS[xs64_below(&rng, 2)]);
            } else {
                mpfr_set_nan(a);
            }
            /* Populate b. */
            if (kb < 7) {
                const double mag = (double)(1 + xs64_below(&rng, 1000000));
                const int sb = SIGNS[xs64_below(&rng, 2)];
                mpfr_set_d(b, sb * mag, MPFR_RNDN);
                if (!mpfr_regular_p(b)) { mpfr_clear(a); mpfr_clear(b); continue; }
            } else if (kb == 7) {
                mpfr_set_zero(b, SIGNS[xs64_below(&rng, 2)]);
            } else if (kb == 8) {
                mpfr_set_inf(b, SIGNS[xs64_below(&rng, 2)]);
            } else {
                mpfr_set_nan(b);
            }
            emit_case(out, "fuzz", a, b);
            mpfr_clear(a);
            mpfr_clear(b);
        }
    }

    /* mined: 5 — no tswap.c exists; use representative usage patterns. */
    {
        /* Common idiom: swap working values between Ziv-loop iterations. */
        { mpfr_t a, b; mk_norm_d(a, 53, 3.14); mk_norm_d(b, 113, 3.14); emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Swap an "accumulator" with a "scratch". */
        { mpfr_t a, b; mk_norm_d(a, 100, 1.0); mk_zero(b, 200, +1); emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* NaN propagation pattern. */
        { mpfr_t a, b; mk_nan(a, 53); mk_norm_d(b, 53, 1.5); emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Inf/normal pair. */
        { mpfr_t a, b; mk_inf(a, 53, +1); mk_norm_d(b, 53, 2.0); emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* Symmetric prec, mixed signs. */
        { mpfr_t a, b; mk_norm_d(a, 53, 1.5); mk_norm_d(b, 53, -1.5); emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    return 0;
}
