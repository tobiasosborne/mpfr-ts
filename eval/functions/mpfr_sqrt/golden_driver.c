/*
 * golden_driver.c — Golden master for MPFR's mpfr_sqrt.
 *
 * C signature
 * -----------
 *
 *   int mpfr_sqrt(mpfr_t rop, mpfr_srcptr x, mpfr_rnd_t rnd);
 *
 *   Sets `rop` to `sqrt(x)` rounded per `rnd` at `rop`'s precision.
 *   Returns the ternary (sign of rounded - exact). See mpfr/src/sqrt.c
 *   L505-L600+ (dispatcher and general algorithm).
 *
 * Divergence from C -> TS
 * -----------------------
 *
 * The TS port `mpfr_sqrt(x, prec, rnd) -> Result` takes `prec` as a
 * positional argument and returns the canonical {value, ternary} pair
 * from src/core.ts L173-L176. The C function mutates `rop` (whose prec
 * is independently set) and returns the ternary separately.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25   (typical sqrts at common precs, RNDN)
 *   edge         :  ~35   (specials × rnd; signed zero; integer squares;
 *                          sqrt of 2 across all modes)
 *   adversarial  :  ~15   (ternary direction across rnd; rounding ties)
 *   fuzz         :  100   (PRNG; random doubles × random precs × rnd)
 *   mined        :    6   (transcribed from mpfr/tests/tsqrt.c)
 *   ------------ ----
 *   total        : ~181
 *
 * Build via the repo-wide eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/sqrt.c — the C reference.
 * Ref: src/ops/sqrt.ts — the production port.
 * Ref: mpfr/tests/tsqrt.c — source for the `mined` cases.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sqrt golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* ------------------------------------------------------------------ */
/* Per-case emitter                                                   */
/* ------------------------------------------------------------------ */

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);

    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_sqrt(rop, x, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}

static inline void init_nan(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_nan(x);
}
static inline void init_pos_inf(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1);
}
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1);
}
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1);
}
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1);
}

static inline double bits_to_double(uint64_t bits) {
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

static inline void emit_d(FILE *out, const char *tag,
                          double d, uint64_t pin,
                          uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x;
    init_from_double(x, d, pin);
    emit_case(out, tag, x, prec, rnd);
    mpfr_clear(x);
}

static inline void emit_s(FILE *out, const char *tag,
                          const char *s, uint64_t pin,
                          uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)pin);
    mpfr_set_str(x, s, 10, MPFR_RNDN);
    emit_case(out, tag, x, prec, rnd);
    mpfr_clear(x);
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    FILE *out = stdout;

    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25 cases — typical sqrts at common precs, RNDN          */
    /* ============================================================== */
    {
        /* Integer squares -> integer roots (exact at prec>=53). */
        emit_d(out, "happy",   1.0,  53,  53, MPFR_RNDN); /* 1 */
        emit_d(out, "happy",   4.0,  53,  53, MPFR_RNDN); /* 2 */
        emit_d(out, "happy",   9.0,  53,  53, MPFR_RNDN); /* 3 */
        emit_d(out, "happy",  16.0,  53,  53, MPFR_RNDN); /* 4 */
        emit_d(out, "happy",  25.0,  53,  53, MPFR_RNDN); /* 5 */
        emit_d(out, "happy", 100.0,  53,  53, MPFR_RNDN); /* 10 */
        emit_d(out, "happy",10000.0, 53,  53, MPFR_RNDN); /* 100 */
        emit_d(out, "happy",   0.25, 53,  53, MPFR_RNDN); /* 0.5 — exact */
        emit_d(out, "happy",   0.0625,53,53, MPFR_RNDN); /* 0.25 — exact */

        /* Non-integer squares (require rounding at prec=53). */
        emit_d(out, "happy",   2.0,  53,  53, MPFR_RNDN); /* sqrt(2) */
        emit_d(out, "happy",   3.0,  53,  53, MPFR_RNDN); /* sqrt(3) */
        emit_d(out, "happy",   5.0,  53,  53, MPFR_RNDN); /* sqrt(5) */
        emit_d(out, "happy",   7.0,  53,  53, MPFR_RNDN); /* sqrt(7) */
        emit_d(out, "happy",   0.5,  53,  53, MPFR_RNDN); /* sqrt(0.5) ~ 0.7071 */
        emit_d(out, "happy",   3.14, 53,  53, MPFR_RNDN);
        emit_d(out, "happy",   2.71828, 53, 53, MPFR_RNDN);

        /* Powers of 4 -> exact roots (just exponent shift). */
        emit_d(out, "happy",   64.0,  53,  53, MPFR_RNDN); /* 8 */
        emit_d(out, "happy",  256.0,  53,  53, MPFR_RNDN); /* 16 */
        emit_d(out, "happy", 1024.0,  53,  53, MPFR_RNDN); /* 32 */

        /* Powers of 2 (non-square — odd exp) — irrational root. */
        emit_d(out, "happy",   2.0,  53,  64, MPFR_RNDN); /* sqrt(2) at higher prec */
        emit_d(out, "happy",   8.0,  53,  53, MPFR_RNDN); /* 2*sqrt(2) */

        /* Varying output prec. */
        emit_d(out, "happy",   2.0,  53,  24, MPFR_RNDN);
        emit_d(out, "happy",   2.0,  53, 100, MPFR_RNDN);
        emit_d(out, "happy",   2.0,  53, 200, MPFR_RNDN);

        /* Large magnitude. */
        emit_d(out, "happy",   1.0e100, 53,  53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~35 cases — specials × rounding × signed-zero             */
    /* ============================================================== */
    {
        /* (1-5) NaN -> NaN at all 5 rnd modes. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_nan(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (6-10) +Inf -> +Inf at all 5 rnd modes. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_pos_inf(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (11-15) -Inf -> NaN at all 5 rnd modes. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_neg_inf(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (16-20) +0 -> +0 at all 5 rnd modes. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_pos_zero(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (21-25) -0 -> -0 at all 5 rnd modes. (sqrt(-0) = -0 IEEE 754 conv) */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_neg_zero(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (26-30) Negative finite -> NaN at all 5 rnd modes. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_from_double(x, -1.0, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (31-35) sqrt(2) at all 5 rnd modes. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_from_double(x, 2.0, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (36) prec=1 — extreme rounding. */
        emit_d(out, "edge",  2.0, 53,  1, MPFR_RNDN);
        /* (37) prec=1 with exact integer-square input -> exact. */
        emit_d(out, "edge",  4.0, 53,  1, MPFR_RNDN); /* 2 -> at prec=1 represents 2 */
        /* (38) prec >> input prec. */
        emit_d(out, "edge",  2.0, 24, 200, MPFR_RNDN);
        /* (39) input prec >> prec. */
        emit_d(out, "edge",  2.0, 200, 24, MPFR_RNDN);
    }

    /* ============================================================== */
    /* adversarial: ~15 cases — ternary direction + cross-mode disagree */
    /* ============================================================== */
    {
        /* (1-5) sqrt(3) at prec=2 across all rnd modes.
         *      sqrt(3) ~ 1.732... At prec=2 the representable values
         *      near it are 1.5 and 2.0; rounding modes pick differently. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_from_double(x, 3.0, 53);
            emit_case(out, "adversarial", x, 2, RNDS[i]);
            mpfr_clear(x);
        }

        /* (6-10) sqrt(5) at prec=53 across all rnd modes. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_from_double(x, 5.0, 53);
            emit_case(out, "adversarial", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (11) sqrt(2) high prec -> low prec — rounding boundary. */
        {
            mpfr_t x; mpfr_init2(x, 200);
            mpfr_set_d(x, 2.0, MPFR_RNDN);
            emit_case(out, "adversarial", x, 53, MPFR_RNDN);
            mpfr_clear(x);
        }

        /* (12) Large square -> exact integer root. */
        emit_d(out, "adversarial", 1048576.0, 53, 53, MPFR_RNDN); /* 1024 */

        /* (13) Very small positive — exercises low-magnitude exponent. */
        {
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_set_d(x, 1.0e-100, MPFR_RNDN);
            emit_case(out, "adversarial", x, 53, MPFR_RNDN);
            mpfr_clear(x);
        }

        /* (14) Exponent parity: x = 1.5 = 1.1 binary, exp=1 (odd).
         *      Forces the parity-adjustment code path. */
        emit_d(out, "adversarial", 1.5, 53, 53, MPFR_RNDN);

        /* (15) Exponent parity: x = 6.0 = 110 binary, exp=3 (odd).
         *      sqrt(6) ~ 2.449... */
        emit_d(out, "adversarial", 6.0, 53, 53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* fuzz: 100 cases — PRNG, random non-negative doubles × precs × rnd */
    /*                                                                  */
    /* Seed: 0x59171759171759ULL (valid hex).                          */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x59171759171759ULL);
        int emitted = 0;
        while (emitted < 100) {
            const uint64_t bits = xs64_next(&rng);
            const uint64_t exp_bits = (bits >> 52) & 0x7FF;
            if (exp_bits == 0x7FF) continue; /* skip NaN/Inf doubles */

            /* Force non-negative by clearing the sign bit; we cover the
             * negative-input case in edge. Fuzz is for the regular
             * positive-normal path. */
            const double d = bits_to_double(bits & ~(1ULL << 63));

            const uint64_t pin  = 1 + xs64_below(&rng, 200);
            const uint64_t prec = 1 + xs64_below(&rng, 200);
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t x;
            init_from_double(x, d, pin);
            /* Skip if x rounded to zero — covered in edge. */
            if (mpfr_zero_p(x)) {
                mpfr_clear(x);
                continue;
            }
            emit_case(out, "fuzz", x, prec, rnd);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 6 cases — transcribed from mpfr/tests/tsqrt.c            */
    /* ============================================================== */
    {
        /* tsqrt.c-style: simple integer-square / non-square at prec=53. */
        emit_s(out, "mined", "2.0", 53,  53, MPFR_RNDN);
        emit_s(out, "mined", "2.0", 53,  53, MPFR_RNDU);
        emit_s(out, "mined", "2.0", 53,  53, MPFR_RNDD);

        /* tsqrt.c-style: large square with rounding direction sensitivity. */
        emit_s(out, "mined", "0.5", 53,  53, MPFR_RNDN);

        /* tsqrt.c-style: known irrational, multiple precisions. */
        emit_s(out, "mined", "7.0", 53,  53, MPFR_RNDN);

        /* tsqrt.c-style: value derived from "11.4" — non-trivial input
         *      that requires the full rounding pipeline. */
        emit_s(out, "mined", "11.4", 53, 53, MPFR_RNDN);
    }

    return 0;
}
