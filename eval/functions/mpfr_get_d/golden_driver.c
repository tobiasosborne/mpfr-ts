/*
 * golden_driver.c — Golden master for MPFR's mpfr_get_d.
 *
 * C signature
 * -----------
 *
 *   double mpfr_get_d(mpfr_srcptr op, mpfr_rnd_t rnd);
 *
 *   Convert the MPFR value `op` to the closest IEEE 754 binary64 under
 *   `rnd`. No ternary on the get-family. See mpfr/src/get_d.c L34–L132.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The TS port `mpfr_get_d(x, rnd) -> number` takes the immutable MPFR
 * struct from src/core.ts and returns a bare JS number. The C function
 * takes a `mpfr_srcptr`. Both are pure (no mutation, no ternary).
 *
 * To grade the TS port we:
 *   1. Build an mpfr_t at the desired precision via mpfr_init2 +
 *      mpfr_set_*.
 *   2. d = mpfr_get_d(mpfr, rnd) — the operation we mirror.
 *   3. Emit input as the schema-shaped MPFR (via jl_kv_mpfr) and rnd.
 *   4. Emit output as a bare double via jl_output_scalar_double.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":{<MPFR-record>},"rnd":"RND[NZUDA]"},
 *    "output":"<NaN|±Infinity|%.17g>",
 *    "time_ns":<n>}
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25
 *   edge         :  ~50
 *   adversarial  :  ~12
 *   fuzz         :   60
 *   mined        :    5
 *
 * Build via the repo-wide eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/get_d.c — the C reference.
 * Ref: src/ops/get_d.ts — the production port.
 * Ref: mpfr/tests/tget_d.c — source for the `mined` cases.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_get_d golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* ------------------------------------------------------------------ */
/* Per-case emitter — from a constructed mpfr_t                       */
/* ------------------------------------------------------------------ */
/* The bare-double output emit helper jl_output_scalar_double lives in
 * common.h alongside jl_output_scalar_u64 and jl_output_scalar_int. */

/* Emit one mpfr_get_d golden case. The input MPFR `x` must already be
 * constructed. We time only the get_d call. */
static inline void emit_case_mpfr(FILE *out, const char *tag,
                                  mpfr_srcptr x, mpfr_rnd_t rnd) {
    const uint64_t t0 = now_ns();
    const double result = mpfr_get_d(x, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_scalar_double(out, result);
    jl_finish(out, elapsed);
}

/* Convenience: emit a case from a double round-tripped through MPFR at
 * the given prec. The "set_d then get_d" path is the most common one
 * since it produces bit-identical round-trips for prec ≥ 53. */
static inline void emit_case_from_double(FILE *out, const char *tag,
                                         double d, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
    emit_case_mpfr(out, tag, x, rnd);
    mpfr_clear(x);
}

/* Convenience: emit a case from a string representation at the given
 * prec via mpfr_set_str_binary. Used for boundary cases (e.g. "1E1024",
 * "1e-1074") that don't fit in a C double. */
static inline void emit_case_from_str_binary(FILE *out, const char *tag,
                                             const char *s, uint64_t prec,
                                             mpfr_rnd_t rnd) {
    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_str(x, s, 2, MPFR_RNDN);
    emit_case_mpfr(out, tag, x, rnd);
    mpfr_clear(x);
}

/* Construct a -0 double via bit-level memcpy (robust under -ffast-math). */
static inline double make_neg_zero(void) {
    const uint64_t bits = (uint64_t)1 << 63;
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

/* Construct an MPFR singular value. */
static inline void set_nan(mpfr_ptr x) { mpfr_set_nan(x); }
static inline void set_pos_inf(mpfr_ptr x) { mpfr_set_inf(x, 1); }
static inline void set_neg_inf(mpfr_ptr x) { mpfr_set_inf(x, -1); }
static inline void set_pos_zero(mpfr_ptr x) { mpfr_set_zero(x, 1); }
static inline void set_neg_zero(mpfr_ptr x) { mpfr_set_zero(x, -1); }

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25 cases — typical MPFR values that round-trip to double */
    /* ============================================================== */
    {
        /* Small integers round-trip exactly at prec >= ceil(log2)+1. */
        emit_case_from_double(out, "happy", 1.0,    53, MPFR_RNDN);
        emit_case_from_double(out, "happy", 2.0,    53, MPFR_RNDN);
        emit_case_from_double(out, "happy", 3.0,    53, MPFR_RNDN);
        emit_case_from_double(out, "happy", 10.0,   53, MPFR_RNDN);
        emit_case_from_double(out, "happy", 100.0,  53, MPFR_RNDN);
        emit_case_from_double(out, "happy", 1e9,    53, MPFR_RNDN);
        emit_case_from_double(out, "happy", -1.0,   53, MPFR_RNDN);
        emit_case_from_double(out, "happy", -42.0,  53, MPFR_RNDN);

        /* Dyadic fractions — exact at any prec ≥ 1. */
        emit_case_from_double(out, "happy", 0.5,    53, MPFR_RNDN);
        emit_case_from_double(out, "happy", 0.25,   53, MPFR_RNDN);
        emit_case_from_double(out, "happy", 0.125,  53, MPFR_RNDN);

        /* Non-dyadic at prec=53 — round-trip exact (the double IS its
         * canonical 53-bit representative). */
        emit_case_from_double(out, "happy", 3.14,    53, MPFR_RNDN);
        emit_case_from_double(out, "happy", 1.0/3.0, 53, MPFR_RNDN);

        /* Larger / smaller magnitudes in the normal-double range. */
        emit_case_from_double(out, "happy", 1.5e100,  53, MPFR_RNDN);
        emit_case_from_double(out, "happy", -2.5e-100, 53, MPFR_RNDN);
        emit_case_from_double(out, "happy", 6.022e23, 53, MPFR_RNDN);
        emit_case_from_double(out, "happy", 2.718281828459045, 53, MPFR_RNDN);
        emit_case_from_double(out, "happy", 1.4142135623730951, 53, MPFR_RNDN);
        emit_case_from_double(out, "happy", -3.141592653589793, 53, MPFR_RNDN);

        /* Higher prec — the source MPFR has more bits than a double; the
         * 53-bit rounded result should match Number(set_d(d, 53).value)
         * since d's canonical 53-bit form padded with zeros to wider
         * prec rounds back to itself. */
        emit_case_from_double(out, "happy", 3.14,    100, MPFR_RNDN);
        emit_case_from_double(out, "happy", 3.14,    200, MPFR_RNDN);
        emit_case_from_double(out, "happy", -1.0e50, 200, MPFR_RNDN);
        emit_case_from_double(out, "happy", 17.0,    24,  MPFR_RNDN);
        emit_case_from_double(out, "happy", 1.0,     1,   MPFR_RNDN);
        emit_case_from_double(out, "happy", -1.0,    1,   MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50 cases — specials × all rnd, subnormals, boundaries   */
    /* ============================================================== */
    {
        /* NaN under every rounding mode → NaN. */
        for (int i = 0; i < 5; i++) {
            mpfr_t x;
            mpfr_init2(x, 53);
            set_nan(x);
            emit_case_mpfr(out, "edge", x, RNDS[i]);
            mpfr_clear(x);
        }

        /* +Inf under every rounding mode → +Infinity. */
        for (int i = 0; i < 5; i++) {
            mpfr_t x;
            mpfr_init2(x, 53);
            set_pos_inf(x);
            emit_case_mpfr(out, "edge", x, RNDS[i]);
            mpfr_clear(x);
        }

        /* -Inf under every rounding mode → -Infinity. */
        for (int i = 0; i < 5; i++) {
            mpfr_t x;
            mpfr_init2(x, 53);
            set_neg_inf(x);
            emit_case_mpfr(out, "edge", x, RNDS[i]);
            mpfr_clear(x);
        }

        /* +0 under every rounding mode → +0. */
        for (int i = 0; i < 5; i++) {
            mpfr_t x;
            mpfr_init2(x, 53);
            set_pos_zero(x);
            emit_case_mpfr(out, "edge", x, RNDS[i]);
            mpfr_clear(x);
        }

        /* -0 under every rounding mode → -0 (sign preserved). */
        for (int i = 0; i < 5; i++) {
            mpfr_t x;
            mpfr_init2(x, 53);
            set_neg_zero(x);
            emit_case_mpfr(out, "edge", x, RNDS[i]);
            mpfr_clear(x);
        }

        /* Subnormal doubles round-trip via MPFR. set_d(DBL_TRUE_MIN)
         * at prec=53 builds an MPFR exactly representing 2^-1074;
         * get_d at all rounding modes must return DBL_TRUE_MIN. */
        emit_case_from_double(out, "edge", DBL_TRUE_MIN, 53, MPFR_RNDN);
        emit_case_from_double(out, "edge", DBL_TRUE_MIN, 53, MPFR_RNDU);
        emit_case_from_double(out, "edge", DBL_TRUE_MIN, 53, MPFR_RNDD);
        emit_case_from_double(out, "edge", -DBL_TRUE_MIN, 53, MPFR_RNDN);
        emit_case_from_double(out, "edge", DBL_MIN, 53, MPFR_RNDN);
        emit_case_from_double(out, "edge", DBL_MIN / 2.0, 53, MPFR_RNDN);
        emit_case_from_double(out, "edge", DBL_MIN / 4.0, 53, MPFR_RNDN);
        emit_case_from_double(out, "edge", DBL_TRUE_MIN * 3.0, 53, MPFR_RNDN);
        emit_case_from_double(out, "edge", DBL_TRUE_MIN * 5.0, 53, MPFR_RNDN);

        /* Subnormal at higher prec: the MPFR exactly stores the
         * subnormal value padded with zeros; the round-back to 53 is
         * exact, and from there to the IEEE 754 subnormal grid. */
        emit_case_from_double(out, "edge", DBL_TRUE_MIN, 200, MPFR_RNDN);
        emit_case_from_double(out, "edge", DBL_MIN / 8.0, 200, MPFR_RNDN);

        /* prec=1 exercises the value formula's tightest mantissa.
         * 1.0 at prec=1 is exactly 2^0 with mant=1, exp=1. */
        emit_case_from_double(out, "edge", 1.0,  1, MPFR_RNDN);
        emit_case_from_double(out, "edge", 0.5,  1, MPFR_RNDN);
        emit_case_from_double(out, "edge", -1.0, 1, MPFR_RNDN);

        /* prec=53 = exact match. */
        emit_case_from_double(out, "edge", 3.14, 53, MPFR_RNDZ);
        emit_case_from_double(out, "edge", 3.14, 53, MPFR_RNDU);
        emit_case_from_double(out, "edge", 3.14, 53, MPFR_RNDD);
        emit_case_from_double(out, "edge", 3.14, 53, MPFR_RNDA);

        /* prec=200 with all rounding modes — values where rounding
         * 200 → 53 matters. We construct an MPFR at high prec from
         * sqrt(2) computed at high prec, which differs from the
         * double's canonical repr beyond bit 53. */
        {
            mpfr_t x;
            mpfr_init2(x, 200);
            mpfr_sqrt_ui(x, 2, MPFR_RNDN);  /* exact within 200 bits */
            emit_case_mpfr(out, "edge", x, MPFR_RNDN);
            emit_case_mpfr(out, "edge", x, MPFR_RNDZ);
            emit_case_mpfr(out, "edge", x, MPFR_RNDU);
            emit_case_mpfr(out, "edge", x, MPFR_RNDD);
            emit_case_mpfr(out, "edge", x, MPFR_RNDA);
            mpfr_clear(x);
        }

        /* DBL_MAX (the largest finite double, exp_mpfr = 1024). */
        emit_case_from_double(out, "edge",  DBL_MAX, 53, MPFR_RNDN);
        emit_case_from_double(out, "edge", -DBL_MAX, 53, MPFR_RNDN);

        /* Overflow: MPFR value > DBL_MAX. */
        emit_case_from_str_binary(out, "edge",  "1E1024", 53, MPFR_RNDN);
        emit_case_from_str_binary(out, "edge",  "1E1024", 53, MPFR_RNDZ);
        emit_case_from_str_binary(out, "edge",  "1E1024", 53, MPFR_RNDU);
        emit_case_from_str_binary(out, "edge",  "1E1024", 53, MPFR_RNDD);
        emit_case_from_str_binary(out, "edge", "-1E1024", 53, MPFR_RNDN);
        emit_case_from_str_binary(out, "edge", "-1E1024", 53, MPFR_RNDZ);
        emit_case_from_str_binary(out, "edge", "-1E1024", 53, MPFR_RNDU);
        emit_case_from_str_binary(out, "edge", "-1E1024", 53, MPFR_RNDD);

        /* Underflow: MPFR value < 2^-1074. */
        emit_case_from_str_binary(out, "edge",  "1E-1075", 53, MPFR_RNDN);
        emit_case_from_str_binary(out, "edge",  "1E-1075", 53, MPFR_RNDU);
        emit_case_from_str_binary(out, "edge",  "1E-1075", 53, MPFR_RNDD);
        emit_case_from_str_binary(out, "edge",  "1E-1075", 53, MPFR_RNDZ);
        emit_case_from_str_binary(out, "edge", "-1E-1075", 53, MPFR_RNDN);
        emit_case_from_str_binary(out, "edge", "-1E-1075", 53, MPFR_RNDD);
        emit_case_from_str_binary(out, "edge", "-1E-1075", 53, MPFR_RNDU);
    }

    /* ============================================================== */
    /* adversarial: ~12 cases — RNDN ties + cross-mode + carry-out    */
    /* ============================================================== */
    {
        /* Build an MPFR at prec=54 with value exactly equal to
         * a 53-bit value + half ULP. This forces a RNDN tie when
         * rounding to 53 bits. */
        mpfr_t x;
        mpfr_init2(x, 54);
        /* 1.0 + 2^-53 + 2^-54 = (2^54 + 2 + 1) * 2^-54 = (1.5 * 2^-53)
         * + 1: choose a clean tie. Actually we want a number whose
         * 54-bit mantissa is XXX0_10000... so dropping 1 bit hits the
         * half-ulp exactly. */
        mpfr_set_str(x, "1.000000000000000111111111111111111111111111111111111111", 2, MPFR_RNDN);
        emit_case_mpfr(out, "adversarial", x, MPFR_RNDN);
        emit_case_mpfr(out, "adversarial", x, MPFR_RNDZ);
        emit_case_mpfr(out, "adversarial", x, MPFR_RNDU);
        emit_case_mpfr(out, "adversarial", x, MPFR_RNDD);
        mpfr_clear(x);

        /* Carry-out at MSB on rounding: 2 - 2^-54 at prec=54, when
         * rounded RNDU to 53 bits, carries to 2.0 (mantissa wraps,
         * exp bumps). */
        mpfr_init2(x, 54);
        mpfr_set_str(x, "1.111111111111111111111111111111111111111111111111111111", 2, MPFR_RNDN);
        emit_case_mpfr(out, "adversarial", x, MPFR_RNDU);
        emit_case_mpfr(out, "adversarial", x, MPFR_RNDA);
        emit_case_mpfr(out, "adversarial", x, MPFR_RNDN);
        mpfr_clear(x);

        /* Subnormal boundary: source slightly above DBL_MIN, prec=200
         * — exercises rounding to a subnormal target (the source is
         * normal, so the round target depends on exact bits). */
        mpfr_init2(x, 200);
        mpfr_set_str(x, "1.00000000000000000000000000000000000000000000000001E-1022", 2, MPFR_RNDN);
        emit_case_mpfr(out, "adversarial", x, MPFR_RNDN);
        emit_case_mpfr(out, "adversarial", x, MPFR_RNDZ);
        emit_case_mpfr(out, "adversarial", x, MPFR_RNDU);
        mpfr_clear(x);

        /* Right at DBL_MAX: setting an MPFR at high prec to a value
         * just above DBL_MAX should overflow under RNDN/RNDU but
         * stay at DBL_MAX under RNDZ/RNDD. */
        mpfr_init2(x, 200);
        mpfr_set_str(x, "1.11111111111111111111111111111111111111111111111111111E1023", 2, MPFR_RNDN);
        /* That's DBL_MAX with 55 ones — slightly above DBL_MAX. */
        emit_case_mpfr(out, "adversarial", x, MPFR_RNDZ);
        emit_case_mpfr(out, "adversarial", x, MPFR_RNDU);
        mpfr_clear(x);
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG, random MPFR × random prec × random rnd  */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x6E70065D070ULL);
        int emitted = 0;
        while (emitted < 60) {
            const uint64_t bits = xs64_next(&rng);
            const uint64_t exp_field = (bits >> 52) & 0x7FF;
            /* Skip NaN/Inf patterns — covered in edge. */
            if (exp_field == 0x7FF) continue;

            double d;
            memcpy(&d, &bits, sizeof d);

            /* prec uniformly in [1, 256]. Mix narrow (round-out) and
             * wide (round-in) cases. */
            const uint64_t prec = 1 + xs64_below(&rng, 256);
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t x;
            mpfr_init2(x, (mpfr_prec_t)prec);
            mpfr_set_d(x, d, MPFR_RNDN);
            emit_case_mpfr(out, "fuzz", x, rnd);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — transcribed from mpfr/tests/tget_d.c          */
    /* ============================================================== */
    {
        /* tget_d.c L74–L82: set 1e-1075 (binary), get_d RNDA → DBL_TRUE_MIN. */
        emit_case_from_str_binary(out, "mined", "1E-1075", 64, MPFR_RNDA);

        /* tget_d.c L98–L101: inf positive → +Infinity under RNDZ. */
        {
            mpfr_t x;
            mpfr_init2(x, 123);
            set_pos_inf(x);
            emit_case_mpfr(out, "mined", x, MPFR_RNDZ);
            mpfr_clear(x);
        }

        /* tget_d.c L103–L106: inf negative → -Infinity under RNDZ. */
        {
            mpfr_t x;
            mpfr_init2(x, 123);
            set_neg_inf(x);
            emit_case_mpfr(out, "mined", x, MPFR_RNDZ);
            mpfr_clear(x);
        }

        /* tget_d.c L142–L146: -1E1024 (binary) under RNDZ → -DBL_MAX. */
        emit_case_from_str_binary(out, "mined", "-1E1024", 64, MPFR_RNDZ);

        /* tget_d.c L154–L158: 1E1024 (binary) under RNDD → DBL_MAX. */
        emit_case_from_str_binary(out, "mined", "1E1024", 64, MPFR_RNDD);
    }

    return 0;
}
