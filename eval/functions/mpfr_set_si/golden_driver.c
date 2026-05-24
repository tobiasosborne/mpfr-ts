/*
 * golden_driver.c — Golden master for MPFR's mpfr_set_si.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_si(mpfr_t rop, long int op, mpfr_rnd_t rnd);
 *
 *   Stores `op` into `rop` rounded per `rnd`. Returns the ternary
 *   (sign of rounded - exact). Ref: mpfr/src/set_si.c L25–L29 →
 *   mpfr/src/set_si_2exp.c L27–L92.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The TS port `mpfr_set_si(n, prec, rnd) -> Result` takes the integer
 * as a `bigint` (because JS Number can't hold the full int64 range)
 * and returns the canonical {value, ternary} pair.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"n":"<int64-decimal>","prec":"<prec-decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 *   - `n` is emitted via jl_kv_i64 (a quoted signed decimal string)
 *     so the TS-side decodeInputValue turns it into a BigInt via
 *     value_codec.ts's isDecimalIntegerString branch. Required for the
 *     int64 magnitudes that exceed Number.MAX_SAFE_INTEGER.
 *   - `prec` is jl_kv_u64.
 *   - `rnd` is jl_kv_rnd.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums, ≥150 total)
 * --------------------------------------------------------
 *
 *   happy        :  ~25   (small ints at common precs, all 5 rnd modes
 *                          implicit through subset; ternary 0 for
 *                          prec >= bitLength(|n|))
 *   edge         :  ~50   (LONG_MIN, LONG_MAX, ±2^k boundaries, prec=1
 *                          forcing rounding, n=0 force sign +1)
 *   adversarial  :  ~30   (large |n| at small prec stressing all 5
 *                          rounding modes; ties on RNDN)
 *   fuzz         :   55   (PRNG-driven; xs64 seed 0x5E715E715E715E71ULL)
 *   mined        :    5   (transcribed from mpfr/tests/tset_si.c)
 *
 * NaN / Inf / out-of-range integer inputs are NOT emitted — the TS port
 * throws on those (divergence from C) and the harness scores throws as
 * n_throw; covered by unit inspection of src/ops/set_si.ts instead.
 *
 * Build via eval/golden_master/build.sh.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_si golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Mirror src/core.ts PREC_MAX / PREC_MIN. */
#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Build-time guard: the TS port assumes int64 = `long`. On platforms
 * where long != 8 bytes (e.g. Win64 ILP32 long), the golden would
 * silently truncate the range we want to exercise. Linux x86_64 /
 * aarch64 have 8-byte long; assert so. */
_Static_assert(sizeof(long) == 8, "mpfr_set_si golden requires 64-bit long");

/* Emit one mpfr_set_si golden case.
 *
 *   1. mpfr_init2(rop, prec).
 *   2. ternary = mpfr_set_si(rop, n, rnd).
 *   3. emit {tag, inputs:{n, prec, rnd}, output:{value, ternary}}.
 *
 * Timing brackets only the mpfr_set_si call. */
static inline void emit_case(FILE *out, const char *tag,
                             long n, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_set_si(rop, n, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_i64(out, 1, "n", (int64_t)n);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25 cases — small ints at common precs.                 */
    /* ============================================================== */
    {
        /* Zero (forces +0 per the C reference). */
        emit_case(out, "happy", 0, 53, MPFR_RNDN);
        emit_case(out, "happy", 0, 24, MPFR_RNDN);
        emit_case(out, "happy", 0, 1, MPFR_RNDN);

        /* Small magnitudes, both signs, at IEEE float64 prec. */
        emit_case(out, "happy",  1, 53, MPFR_RNDN);
        emit_case(out, "happy", -1, 53, MPFR_RNDN);
        emit_case(out, "happy",  2, 53, MPFR_RNDN);
        emit_case(out, "happy", -2, 53, MPFR_RNDN);
        emit_case(out, "happy",  10, 53, MPFR_RNDN);
        emit_case(out, "happy", -10, 53, MPFR_RNDN);
        emit_case(out, "happy",  100, 53, MPFR_RNDN);
        emit_case(out, "happy", -100, 53, MPFR_RNDN);
        emit_case(out, "happy",  1000, 53, MPFR_RNDN);
        emit_case(out, "happy", -1000, 53, MPFR_RNDN);
        emit_case(out, "happy",  1000000000L, 53, MPFR_RNDN);
        emit_case(out, "happy", -1000000000L, 53, MPFR_RNDN);

        /* Common precs at various magnitudes. */
        emit_case(out, "happy",  42, 24, MPFR_RNDN);
        emit_case(out, "happy", -42, 64, MPFR_RNDN);
        emit_case(out, "happy",  1234567, 100, MPFR_RNDN);
        emit_case(out, "happy", -1234567, 200, MPFR_RNDN);
        emit_case(out, "happy",  3, 53, MPFR_RNDN);
        emit_case(out, "happy",  5, 53, MPFR_RNDN);
        emit_case(out, "happy",  7, 53, MPFR_RNDN);
        emit_case(out, "happy",  255, 53, MPFR_RNDN);
        emit_case(out, "happy",  256, 53, MPFR_RNDN);
        emit_case(out, "happy",  65535, 53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50 cases — LONG boundaries, ±2^k, prec=1, n=0 across    */
    /* all 5 rnd modes (signed-zero stability check).                 */
    /* ============================================================== */
    {
        /* (1-5) n=0 at all 5 rnd modes (should always give +0). */
        for (int i = 0; i < 5; ++i) {
            emit_case(out, "edge", 0, 53, RNDS[i]);
        }

        /* (6-10) LONG_MAX at all 5 rnd modes at prec=53 (forces
         * rounding since LONG_MAX = 2^63 - 1 has 63 bits). */
        for (int i = 0; i < 5; ++i) {
            emit_case(out, "edge", LONG_MAX, 53, RNDS[i]);
        }

        /* (11-15) LONG_MIN at all 5 rnd modes at prec=53 (LONG_MIN =
         * -2^63 has 64 bits in absolute value, forces rounding at
         * prec=53). */
        for (int i = 0; i < 5; ++i) {
            emit_case(out, "edge", LONG_MIN, 53, RNDS[i]);
        }

        /* (16-20) LONG_MAX at prec=63 — exact, since LONG_MAX has 63
         * bits. */
        for (int i = 0; i < 5; ++i) {
            emit_case(out, "edge", LONG_MAX, 63, RNDS[i]);
        }

        /* (21-25) LONG_MIN at prec=64 — exact at prec 64. */
        for (int i = 0; i < 5; ++i) {
            emit_case(out, "edge", LONG_MIN, 64, RNDS[i]);
        }

        /* (26-30) ±1 at prec=1 — exact (1 bit suffices for ±1). */
        emit_case(out, "edge",  1, 1, MPFR_RNDN);
        emit_case(out, "edge", -1, 1, MPFR_RNDN);
        emit_case(out, "edge",  1, 1, MPFR_RNDZ);
        emit_case(out, "edge", -1, 1, MPFR_RNDU);
        emit_case(out, "edge",  1, 1, MPFR_RNDA);

        /* (31-35) Powers of 2 — exact at any prec ≥ 1 since |2^k| has
         * only one set bit (the MSB). */
        emit_case(out, "edge",  1L << 10, 1, MPFR_RNDN);
        emit_case(out, "edge",  1L << 30, 1, MPFR_RNDN);
        emit_case(out, "edge",  1L << 62, 1, MPFR_RNDN);
        emit_case(out, "edge", -(1L << 10), 1, MPFR_RNDN);
        emit_case(out, "edge", -(1L << 62), 1, MPFR_RNDN);

        /* (36-40) 2^k - 1 (all bits set in lower k) at prec ≤ k - 1
         * (forces full-mantissa rounding). */
        emit_case(out, "edge",  (1L << 10) - 1, 5, MPFR_RNDN);
        emit_case(out, "edge",  (1L << 20) - 1, 10, MPFR_RNDN);
        emit_case(out, "edge",  (1L << 30) - 1, 20, MPFR_RNDU);
        emit_case(out, "edge", -((1L << 30) - 1), 20, MPFR_RNDD);
        emit_case(out, "edge",  (1L << 50) - 1, 40, MPFR_RNDA);

        /* (41-45) Just-past-boundary values. */
        emit_case(out, "edge", LONG_MAX - 1, 53, MPFR_RNDN);
        emit_case(out, "edge", LONG_MIN + 1, 53, MPFR_RNDN);
        emit_case(out, "edge",  (1L << 53), 53, MPFR_RNDN);     /* 53 bits + 0 → exact */
        emit_case(out, "edge",  (1L << 53) + 1, 53, MPFR_RNDN); /* 54 bits → forces round */
        emit_case(out, "edge", -((1L << 53) + 1), 53, MPFR_RNDN);

        /* (46-50) Prec edge cases. We avoid TS_PREC_MAX here because
         * mpfr_init2 at 2^31-257 bits allocates ~256MB and slows the
         * golden generator to a crawl; a 4096-bit prec is comfortably
         * over the practical range while keeping per-case time
         * sub-millisecond. */
        emit_case(out, "edge",  42, TS_PREC_MIN, MPFR_RNDN);
        emit_case(out, "edge", -42, TS_PREC_MIN, MPFR_RNDN);
        emit_case(out, "edge",  42, 4096, MPFR_RNDN);
        emit_case(out, "edge",  3, 1, MPFR_RNDN); /* 3 = 0b11; prec=1 rounds to 4 RNDN ties-to-even */
        emit_case(out, "edge",  3, 1, MPFR_RNDZ); /* prec=1 truncates 0b11 → 0b10 = 2 */
    }

    /* ============================================================== */
    /* adversarial: ~30 cases — all 5 rnd modes × 6 inexact-rounding  */
    /* patterns; RNDN ties on signed values.                          */
    /* ============================================================== */
    {
        /* Inexact-rounding patterns: integers whose bit patterns are
         * crafted so different rnd modes produce different rounded
         * mantissas at prec < bitLength. Each pattern × 5 modes ×
         * both signs.
         *
         * Pattern 0b11011 (27) at prec=3: bitLength=5, drops 2 bits
         * (the "11" tail). Half-ulp boundary is exactly the dropped
         * "10" pattern; "11" is strictly above the half → RNDN rounds
         * up; RNDZ/RNDD positive truncate, RNDA/RNDU positive
         * increment.
         *
         * Pattern 0b10101 (21) at prec=3: dropped "01" is below half
         * → RNDN truncates; RNDA/RNDU positive increment. */
        const long patterns[] = {
            0b11011L,        /* 27 */
            0b10101L,        /* 21 */
            0b11111L,        /* 31 — RNDA carry-out candidate */
            0b10001L,        /* 17 — dropped low bit, far from half */
            0b11100L,        /* 28 — dropped "00" exact at prec=3 */
        };
        const size_t n_pat = sizeof(patterns) / sizeof(patterns[0]);
        for (size_t p = 0; p < n_pat; ++p) {
            for (int r = 0; r < 5; ++r) {
                emit_case(out, "adversarial",  patterns[p], 3, RNDS[r]);
                emit_case(out, "adversarial", -patterns[p], 3, RNDS[r]);
            }
        }

        /* RNDN tie cases. Source bit pattern "110" → drop a half-bit
         * "1" at prec=2: tie between 0b10 (=2) and 0b11 (=3, but that
         * would carry to 0b100=4; with prec=2 we round 0b110 to 2
         * bits = either 0b10=2 (lsb 0, even) or 0b100=4 (lsb 0, even
         * after carry). Both candidates are even — that's a
         * malformed tie example. Use the standard pattern: 0b1010
         * at prec=2, tie between 0b10 and 0b11. */
        emit_case(out, "adversarial",  0b1010, 2, MPFR_RNDN);  /* even-LSB → no inc */
        emit_case(out, "adversarial", -0b1010, 2, MPFR_RNDN);
        emit_case(out, "adversarial",  0b1110, 2, MPFR_RNDN);  /* odd-LSB → inc */
        emit_case(out, "adversarial", -0b1110, 2, MPFR_RNDN);
    }

    /* ============================================================== */
    /* fuzz: 55 cases — PRNG-driven over the int64 range.             */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5E715E715E715E71ULL);
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        for (int rep = 0; rep < 55; ++rep) {
            /* Random int64 spanning the full range. xs64_next returns
             * uint64_t; reinterpret as int64_t to get signed values. */
            const uint64_t u = xs64_next(&rng);
            int64_t n;
            memcpy(&n, &u, sizeof n);

            const uint64_t prec = precs[xs64_below(&rng, 6)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            emit_case(out, "fuzz", (long)n, prec, rnd);
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — from mpfr/tests/tset_si.c                     */
    /* ============================================================== */
    {
        /* tset_si.c L60–L62: mpfr_set_si(x, -1, RNDN); check x == -1.
         * We emit the same triple. */
        emit_case(out, "mined", -1, 53, MPFR_RNDN);
        /* tset_si.c L66–L68: mpfr_set_si(x, 1, RNDN). */
        emit_case(out, "mined",  1, 53, MPFR_RNDN);
        /* tset_si.c L74–L76: mpfr_set_si(x, 1024, RNDN). */
        emit_case(out, "mined", 1024, 53, MPFR_RNDN);
        /* tset_si.c L82–L84: mpfr_set_si(x, LONG_MAX, RNDN). */
        emit_case(out, "mined", LONG_MAX, 53, MPFR_RNDN);
        /* tset_si.c L88–L91: mpfr_set_si(x, LONG_MIN, RNDN). */
        emit_case(out, "mined", LONG_MIN, 53, MPFR_RNDN);
    }

    return 0;
}
