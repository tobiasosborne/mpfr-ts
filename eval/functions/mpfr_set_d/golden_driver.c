/*
 * golden_driver.c — Golden master for MPFR's mpfr_set_d.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_d(mpfr_t rop, double op, mpfr_rnd_t rnd);
 *
 *   Convert the IEEE 754 binary64 `op` to an MPFR value at `rop`'s
 *   precision, rounded per `rnd`, returning the ternary flag (sign of
 *   rounded - exact). See mpfr/src/set_d.c L241–L324.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The TS port `mpfr_set_d(d, prec, rnd) -> Result` takes `prec` as a
 * positional argument and returns the canonical {value, ternary} pair
 * from src/core.ts L173–L176. The C function mutates `rop` (whose prec
 * is independently set) and returns the ternary separately.
 *
 * To grade the TS port we therefore:
 *   1. mpfr_init2(probe, prec)               — set up an mpfr_t at prec.
 *   2. ternary = mpfr_set_d(probe, d, rnd)   — the operation we mirror.
 *   3. emit Result-shaped output via jl_output_result(probe, ternary).
 *   4. mpfr_clear(probe).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"d":"<NaN|±Infinity|%.17g>","prec":"<decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 *   - `d` via jl_kv_double — quoted string ("%.17g" for finite, sentinel
 *     for specials). The TS decoder in value_codec.ts recognises the
 *     special tokens and the %.17g-finite case, parsing both into a JS
 *     `number`.
 *   - `prec` via jl_kv_u64 — decimal bigint string.
 *   - `rnd`  via jl_kv_rnd — RoundingMode string.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25  (typical doubles, common precisions, RNDN)
 *   edge         :  ~50  (specials × all 5 rounding modes; subnormals;
 *                         prec=1 / prec=53 / prec=200; ±0; MIN/MAX_VALUE)
 *   adversarial  :  ~16  (RNDN tie boundaries; cross-mode disagreement;
 *                         carry-out at MSB)
 *   fuzz         :  100  (PRNG; uniform exponent in [-1022, 1023]; random
 *                         mantissa bits; random prec in [1, 256]; random
 *                         rnd; rejection of NaN-pattern bits)
 *   mined        :   5  (transcribed from mpfr/tests/tset_d.c)
 *   ------------ ----
 *   total        : ~196
 *
 * Build
 * -----
 *
 *   gcc -O2 -std=c11 -Wall -Wextra -Werror -I../../golden_master \
 *       golden_driver.c $(pkg-config --cflags --libs mpfr) -lgmp -lm \
 *       -o golden_driver
 *
 * The repo-wide eval/golden_master/build.sh finds and builds this file
 * automatically.
 *
 * Ref: mpfr/src/set_d.c — the C reference.
 * Ref: src/core.ts L19–L63 — MPFR value model the wire round-trips through.
 * Ref: src/ops/set_d.ts — the production port.
 * Ref: mpfr/tests/tset_d.c — source for the `mined` cases.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Compile-time invariants                                            */
/* ------------------------------------------------------------------ */

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_d golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Mirror src/core.ts L236: PREC_MAX = 2^31 - 257. We emit prec values
 * in [1, PREC_MAX]; the production port's validateArgs rejects
 * out-of-range values, and the golden cannot exercise expected-throw
 * cases (Rule 7). */
#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* ------------------------------------------------------------------ */
/* Per-case emitter                                                   */
/* ------------------------------------------------------------------ */

/* Emit one mpfr_set_d golden case.
 *
 * Steps:
 *   1. mpfr_init2(probe, prec)              — at TS prec bound.
 *   2. ternary = mpfr_set_d(probe, d, rnd)  — time only this call.
 *   3. emit {tag, inputs:{d, prec, rnd}, output:{value: probe, ternary}}.
 *   4. mpfr_clear.
 */
static inline void emit_case(FILE *out, const char *tag,
                             double d, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);

    mpfr_t probe;
    mpfr_init2(probe, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_set_d(probe, d, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_double(out, 1, "d", d);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, probe, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(probe);
}

/* ------------------------------------------------------------------ */
/* Special-value generators                                           */
/* ------------------------------------------------------------------ */

/* Build a -0 explicitly via memcpy of the bit pattern. `-0.0` in a C
 * literal is correctly negative on every IEEE-754 host, but some older
 * compilers fold `-0.0` to `0.0` under -ffast-math; we use a bit-level
 * construction to be robust. */
static inline double make_neg_zero(void) {
    const uint64_t bits = (uint64_t)1 << 63;
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

/* Construct a double from raw IEEE 754 bits — the standard
 * "type-pun via memcpy" idiom; UB-free per C11 §6.5p7. Used to mint
 * adversarial values whose mantissa lies on a rounding tie boundary. */
static inline double bits_to_double(uint64_t bits) {
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

/* Read a double's IEEE 754 bit pattern. */
static inline uint64_t double_to_bits(double d) {
    uint64_t bits;
    memcpy(&bits, &d, sizeof bits);
    return bits;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    FILE *out = stdout;

    /* All five rounding modes — used in cross-mode edge/adversarial cases. */
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: 25 cases — typical doubles, common precisions, RNDN     */
    /* ============================================================== */
    {
        /* Small integers (exact at any prec >= ceil(log2 + 1) bits). */
        emit_case(out, "happy", 1.0,     53, MPFR_RNDN);
        emit_case(out, "happy", 2.0,     53, MPFR_RNDN);
        emit_case(out, "happy", 3.0,     53, MPFR_RNDN);
        emit_case(out, "happy", 10.0,    53, MPFR_RNDN);
        emit_case(out, "happy", 100.0,   53, MPFR_RNDN);
        emit_case(out, "happy", 1e9,     53, MPFR_RNDN);
        emit_case(out, "happy", -1.0,    53, MPFR_RNDN);
        emit_case(out, "happy", -42.0,   53, MPFR_RNDN);

        /* Simple fractions. 0.5, 0.25, 0.125 — dyadic, exact at any
         * prec >= 1. 3.14, 1/3 — not exact in binary, but the double
         * IS the rounded representative, so at prec=53 the conversion
         * is bit-identical (ternary 0). */
        emit_case(out, "happy", 0.5,     53, MPFR_RNDN);
        emit_case(out, "happy", 0.25,    53, MPFR_RNDN);
        emit_case(out, "happy", 0.125,   53, MPFR_RNDN);
        emit_case(out, "happy", 3.14,    53, MPFR_RNDN);
        emit_case(out, "happy", 1.0/3.0, 53, MPFR_RNDN);

        /* Various precisions — 24 (float32), 53, 64 (x86 ld), 100,
         * 113 (float128), 200, 256. All RNDN for tag-class neutrality;
         * other rounding modes are exercised in edge/adversarial. */
        emit_case(out, "happy", 3.14,    24, MPFR_RNDN);
        emit_case(out, "happy", 3.14,    64, MPFR_RNDN);
        emit_case(out, "happy", 3.14,   100, MPFR_RNDN);
        emit_case(out, "happy", 3.14,   113, MPFR_RNDN);
        emit_case(out, "happy", 3.14,   200, MPFR_RNDN);
        emit_case(out, "happy", 3.14,   256, MPFR_RNDN);

        /* Scientific notation, large + small magnitudes. */
        emit_case(out, "happy",  1.5e100,  53, MPFR_RNDN);
        emit_case(out, "happy", -2.5e-100, 53, MPFR_RNDN);
        emit_case(out, "happy",  6.022e23, 53, MPFR_RNDN);  /* Avogadro */
        emit_case(out, "happy",  2.718281828459045, 53, MPFR_RNDN);  /* e */
        emit_case(out, "happy",  1.4142135623730951, 53, MPFR_RNDN); /* sqrt(2) */
        emit_case(out, "happy", -3.141592653589793, 53, MPFR_RNDN);  /* -pi */
    }

    /* ============================================================== */
    /* edge: ~50 cases — specials, all rounding modes, boundaries     */
    /* ============================================================== */
    {
        const double pos_zero = 0.0;
        const double neg_zero = make_neg_zero();

        /* NaN under every rounding mode — should produce NAN_VALUE
         * with ternary 0, prec discarded. */
        for (int i = 0; i < 5; i++) {
            emit_case(out, "edge", (double)NAN, 53, RNDS[i]);
        }

        /* +Inf under every rounding mode. */
        for (int i = 0; i < 5; i++) {
            emit_case(out, "edge", (double)INFINITY, 53, RNDS[i]);
        }

        /* -Inf under every rounding mode. */
        for (int i = 0; i < 5; i++) {
            emit_case(out, "edge", -(double)INFINITY, 53, RNDS[i]);
        }

        /* +0 under every rounding mode. */
        for (int i = 0; i < 5; i++) {
            emit_case(out, "edge", pos_zero, 53, RNDS[i]);
        }

        /* -0 under every rounding mode. The sign MUST survive — RNDD
         * vs RNDU of (+0 + -0) is the canonical observable case, but
         * even on a pure conversion the sign on the output must match
         * the sign on the input. */
        for (int i = 0; i < 5; i++) {
            emit_case(out, "edge", neg_zero, 53, RNDS[i]);
        }

        /* Subnormal doubles. DBL_MIN = 2^-1022 is the smallest normal;
         * DBL_TRUE_MIN / Number.MIN_VALUE = 2^-1074 is the smallest
         * subnormal. The "checks that subnormals are not flushed to
         * zero" loop in tset_d.c L122–L138 exercises 2^-1022-n for
         * n in [0, 52]. We sample a few. */
        emit_case(out, "edge", DBL_MIN,                  53, MPFR_RNDN);
        emit_case(out, "edge", DBL_MIN / 2.0,            53, MPFR_RNDN); /* 2^-1023 */
        emit_case(out, "edge", DBL_MIN / 4.0,            53, MPFR_RNDN); /* 2^-1024 */
        emit_case(out, "edge", DBL_TRUE_MIN,             53, MPFR_RNDN); /* 2^-1074 */
        emit_case(out, "edge", -DBL_TRUE_MIN,            53, MPFR_RNDN);
        emit_case(out, "edge", DBL_TRUE_MIN * 3.0,       53, MPFR_RNDN); /* non-pow2 subnormal */

        /* Subnormal at low prec — exercises rounding of the
         * renormalised subnormal mantissa. */
        emit_case(out, "edge", DBL_TRUE_MIN * 5.0, 2, MPFR_RNDN);
        emit_case(out, "edge", DBL_TRUE_MIN * 7.0, 3, MPFR_RNDD);

        /* MAX_VALUE — biggest finite double, 2^1024 (1 - 2^-53). */
        emit_case(out, "edge", DBL_MAX,  53, MPFR_RNDN);
        emit_case(out, "edge", -DBL_MAX, 53, MPFR_RNDN);
        emit_case(out, "edge", DBL_MAX, 1024, MPFR_RNDN);  /* lossless pad */

        /* Boundary precisions: prec=1 (extreme rounding), prec=52
         * (one bit narrower than double), prec=53 (exact match),
         * prec=54 (one bit pad), prec=200 (broad pad). */
        emit_case(out, "edge", 3.14, 1,   MPFR_RNDN);
        emit_case(out, "edge", 3.14, 52,  MPFR_RNDN);
        emit_case(out, "edge", 3.14, 53,  MPFR_RNDN);
        emit_case(out, "edge", 3.14, 54,  MPFR_RNDN);
        emit_case(out, "edge", 3.14, 200, MPFR_RNDN);

        /* prec=1: 1.0 must stay 1.0 (it's 2^0, MSB-aligned to 1 bit
         * trivially). 1.5 at prec=1 rounds to either 1 or 2 depending
         * on rnd. */
        emit_case(out, "edge", 1.0,  1, MPFR_RNDN);
        emit_case(out, "edge", 1.5,  1, MPFR_RNDN);  /* ties-to-even → 2 */
        emit_case(out, "edge", 1.5,  1, MPFR_RNDZ);  /* → 1 */
        emit_case(out, "edge", 1.5,  1, MPFR_RNDU);  /* → 2 */
        emit_case(out, "edge", 1.5,  1, MPFR_RNDD);  /* → 1 */
        emit_case(out, "edge", 1.5,  1, MPFR_RNDA);  /* → 2 */
        emit_case(out, "edge", -1.5, 1, MPFR_RNDD);  /* → -2 (further from 0) */
        emit_case(out, "edge", -1.5, 1, MPFR_RNDU);  /* → -1 (toward 0 from negative) */
        emit_case(out, "edge", -1.5, 1, MPFR_RNDA);  /* → -2 */

        /* Mined-from-tset_d.c value at very wide prec — exercises the
         * lossless padding path on a value with non-trivial bit pattern. */
        emit_case(out, "edge", -1.08007920352320089721e+150, 53,  MPFR_RNDN);
        emit_case(out, "edge", -1.08007920352320089721e+150, 256, MPFR_RNDN);

        /* Large-magnitude negative at narrow precision — exercises
         * RNDD/RNDU asymmetry. */
        emit_case(out, "edge", -3.14159e10, 8, MPFR_RNDD);
        emit_case(out, "edge", -3.14159e10, 8, MPFR_RNDU);
        emit_case(out, "edge", -3.14159e10, 8, MPFR_RNDA);
        emit_case(out, "edge", -3.14159e10, 8, MPFR_RNDZ);
    }

    /* ============================================================== */
    /* adversarial: ~16 cases — tie boundaries + carry-out + cross-mode */
    /* ============================================================== */
    {
        /* The classic from tset_d.c L142: 5.0 at prec=2, RNDN should
         * round ties-to-even, giving 4.0 (ternary -1). 5 = 0b101;
         * rounding to 2 MSB bits: trunc = 0b10 = 2, dropped = 1 bit
         * = 0.5 (half) — tie. trunc LSB is 0 → round to even = keep
         * trunc → 0b10 << 1 = 4 with exp adjusted. */
        emit_case(out, "adversarial",  5.0, 2, MPFR_RNDN);  /* → 4 */
        emit_case(out, "adversarial", -5.0, 2, MPFR_RNDN);  /* → -4 */

        /* RNDU specific from tset_d.c L157: 0.984... at default prec
         * with RNDU should yield 1.0. Default in the test is 53 bits;
         * at that prec the result IS 0.984... unchanged (the double
         * already IS the rounded representative). The interesting
         * case is at prec=1: 0.984... rounds to 1 under RNDU. */
        emit_case(out, "adversarial", 9.84891017624509146344e-01,  1, MPFR_RNDU);
        emit_case(out, "adversarial", 9.84891017624509146344e-01,  1, MPFR_RNDD);
        emit_case(out, "adversarial", 9.84891017624509146344e-01,  1, MPFR_RNDN);

        /* Carry-out at MSB: 1.999...9 (2 - 2^-52) at prec=1 with RNDU
         * rounds up to 2, which is the carry case (mant 0b11... + 1
         * = 0b100..., MSB shifts up, exp bumps). The TS port has an
         * explicit branch for this; we exercise it.
         *
         * The double bit pattern for 1.999...: exp_biased = 1023
         * (giving unbiased = 0 = MPFR exp 1), mantissa = all 52 ones.
         * = 0x3FFFFFFFFFFFFFFF. */
        const double almost_two = bits_to_double(0x3FFFFFFFFFFFFFFFULL);
        emit_case(out, "adversarial", almost_two, 1, MPFR_RNDU);  /* → 2 */
        emit_case(out, "adversarial", almost_two, 1, MPFR_RNDA);  /* → 2 */
        emit_case(out, "adversarial", almost_two, 1, MPFR_RNDN);  /* → 2 (closer than 1) */
        emit_case(out, "adversarial", almost_two, 2, MPFR_RNDU);  /* trunc=0b11 + 1 = 0b100 carry */
        emit_case(out, "adversarial", -almost_two, 1, MPFR_RNDD); /* → -2 */
        emit_case(out, "adversarial", -almost_two, 2, MPFR_RNDD); /* → -2 (carry) */

        /* Exact-tie at prec=52: drop one mantissa bit. We construct a
         * double whose 53-bit mantissa is `1...10` (LSB 0) and one
         * whose mantissa is `1...11` (LSB 1) to exercise the ties-to-
         * even branch's even/odd LSB sensitivity. With dropped bit
         * being the LSB (a single bit), dropped=1 is "above half"
         * (half is 0 when k=1) — actually for k=1, half = 2^0 = 1,
         * and dropped is in {0, 1}: dropped=0 means exact, dropped=1
         * means strictly above half (no actual tie possible with one
         * dropped bit). So a real RNDN tie needs more bits. We
         * exercise at prec=1 instead. */
        /* 6.0 = 0b110 → at prec=1: trunc=0b1, dropped=0b10. With k=2,
         * half=2, dropped=2 → exact tie, trunc LSB=1 → round up → 0b10
         * = 2 then carry-shift to 0b1 at exp+1, giving 8. Wait — 6 at
         * prec=1 should round to 8 under ties-to-even? Let's check:
         * 6 is between 4 (0b100, prec=1: 0b1 with exp=3) and 8 (0b1000,
         * prec=1: 0b1 with exp=4). Distance to 4 is 2, distance to 8
         * is 2 — tie. 4's mant LSB is the only bit, =1 (since prec=1
         * means single bit which is the MSB which is 1). 8's mant LSB
         * is also 1. Both are "odd" in the round-to-even sense; the
         * tie-break in this case is well-defined by the round_raw
         * convention (we follow libmpfr's output regardless). */
        emit_case(out, "adversarial",  6.0, 1, MPFR_RNDN);
        emit_case(out, "adversarial", -6.0, 1, MPFR_RNDN);

        /* Cross-mode disagreement: 0.1 at prec=10. 0.1 is not dyadic
         * so all five rounding modes may produce slightly different
         * results. Useful as a "broken port producing the same value
         * for all modes" detector. */
        emit_case(out, "adversarial", 0.1, 10, MPFR_RNDN);
        emit_case(out, "adversarial", 0.1, 10, MPFR_RNDZ);
        emit_case(out, "adversarial", 0.1, 10, MPFR_RNDU);
        emit_case(out, "adversarial", 0.1, 10, MPFR_RNDD);
        emit_case(out, "adversarial", 0.1, 10, MPFR_RNDA);
    }

    /* ============================================================== */
    /* fuzz: 100 cases — PRNG, random doubles × random prec × random rnd */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5E70065D070ULL);
        int emitted = 0;
        while (emitted < 100) {
            /* Draw 64 random bits. Reject NaN-pattern bits so we don't
             * spend a fuzz case re-testing the NaN path (already covered
             * in edge); ±Inf is also rejected for the same reason.
             * Specifically: exponent field == 2047 means NaN (mant != 0)
             * or ±Inf (mant == 0). Skip both. */
            const uint64_t bits = xs64_next(&rng);
            const uint64_t exp_field = (bits >> 52) & 0x7FF;
            if (exp_field == 0x7FF) continue;

            const double d = bits_to_double(bits);
            /* prec uniformly in [1, 256] — covers prec<53 (lossy round)
             * and prec>53 (lossless pad) regimes. */
            const uint64_t prec = 1 + xs64_below(&rng, 256);
            /* rnd uniformly over the 5 modes. */
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            emit_case(out, "fuzz", d, prec, rnd);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — transcribed from mpfr/tests/tset_d.c          */
    /* ============================================================== */
    {
        /* tset_d.c L142: mpfr_set_d (x, 5.0, MPFR_RNDN) at prec=2 → 4. */
        emit_case(out, "mined", 5.0, 2, MPFR_RNDN);

        /* tset_d.c L149: mpfr_set_d (x, -5.0, MPFR_RNDN) at prec=2 → -4. */
        emit_case(out, "mined", -5.0, 2, MPFR_RNDN);

        /* tset_d.c L157: mpfr_set_d (x, 9.848...e-01, MPFR_RNDU). At
         * the test's prec=2 this rounds to 1.0 (exp 1, mant 0b10 →
         * MSB-aligned 0b10, value 1*2^0=1 — wait that's mant 0b1 at
         * exp 1 = 1 at prec=1, but at prec=2 the value 1 is 0b10 with
         * exp=1. Either way the value is the integer 1). */
        emit_case(out, "mined", 9.84891017624509146344e-01, 2, MPFR_RNDU);

        /* tset_d.c L166: mpfr_set_d (z, 1.0, RNDN) at prec=32. */
        emit_case(out, "mined", 1.0, 32, MPFR_RNDN);

        /* tset_d.c L175: mpfr_set_d (x, -1.08007920...e+150, RNDN)
         * at prec=53 — must round-trip via mpfr_get_d back to the same
         * double. We can't directly test the round-trip here, but
         * recording the libmpfr output establishes the canonical TS
         * answer the port must match. */
        emit_case(out, "mined", -1.08007920352320089721e+150, 53, MPFR_RNDN);
    }

    return 0;
}
