/*
 * golden_driver.c -- Golden master for MPFR's mpfr_scale2.
 *
 * C signature
 * -----------
 *
 *   double mpfr_scale2(double d, int exp);
 *
 * Multiplies the IEEE-754 binary64 input d by 2^exp. Preconditions
 * (mpfr/src/scale2.c L28-L46):
 *   - 1/2 <= d <= 1  (d == 1 is internally rewritten as d = 0.5, exp += 1)
 *   - -1073 <= exp <= 1025
 *
 * libmpfr-version note
 * --------------------
 *
 * `mpfr_scale2` is exported by libmpfr (visible via nm -D libmpfr.so |
 * grep mpfr_scale2) but not declared in <mpfr.h> (it lives in mpfr-impl.h,
 * which is not installed). The driver re-declares the prototype below.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"d":"<%.17g>","exp":<int>},
 *    "output":"<%.17g>",
 *    "time_ns":<n>}
 *
 *   - `d`: quoted-double via jl_kv_double; the TS decoder produces a JS
 *     number.
 *   - `exp`: bare JS int via jl_kv_int; range [-1073, 1025].
 *   - `output`: quoted-double via jl_output_scalar_double; the TS
 *     comparator uses Object.is for +/-0 and NaN equality (we don't
 *     produce NaN outputs here, but the wire form is uniform).
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  20  (d sampled from [0.5, 1.0); exp in [-10, 10])
 *   edge         :  30  (boundary values: d = 0.5 exactly, d -> 1
 *                        nearly, exp = -1073 / -1022 / -1021 / 0 / 1024 / 1025)
 *   adversarial  :  10  (subnormal-target boundary, exact powers of two
 *                        in d, max/min exp)
 *   fuzz         :  50  (xorshift-driven d in [0.5, 1.0) at random exp
 *                        in [-1073, 1025])
 *   mined        :   0  (mpfr/tests/ has no isolatable mpfr_scale2 test
 *                        driver -- exercised internally via mpfr_get_d
 *                        / mpfr_set_d. Rule 7 carve-out: "or all
 *                        available, if fewer than 5 exist", which here
 *                        is zero.)
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/scale2.c -- C reference.
 */
#include "common.h"

#include <inttypes.h>
#include <math.h>

/* Declared in mpfr-impl.h; symbol exported. */
extern double mpfr_scale2(double d, int exp);

static inline void emit_case(FILE *out, const char *tag, double d, int exp) {
    const uint64_t t0 = now_ns();
    const double result = mpfr_scale2(d, exp);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_double(out, 1, "d", d);
    jl_kv_int(out, 0, "exp", exp);
    jl_end_inputs(out);
    jl_output_scalar_double(out, result);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 20 -- d in [0.5, 1.0), exp in [-10, 10].                */
    /* ============================================================== */
    {
        const double ds[5] = { 0.5, 0.625, 0.75, 0.875, 0.9999 };
        const int exps[4] = { -3, 0, 5, 10 };
        for (int i = 0; i < 5; ++i)
            for (int j = 0; j < 4; ++j)
                emit_case(out, "happy", ds[i], exps[j]);
    }

    /* ============================================================== */
    /* edge: 30 -- boundary d values, boundary exp values.            */
    /* ============================================================== */
    {
        /* d = 0.5 exactly across the full exp range. The d == 1.0
         * rewrite branch is exercised separately via d = 1.0 cases
         * (libmpfr rewrites in-place; we test with d = 1.0 as input). */
        emit_case(out, "edge", 0.5, -1073);
        emit_case(out, "edge", 0.5, -1022);  /* subnormal-target boundary */
        emit_case(out, "edge", 0.5, -1021);  /* first normal target */
        emit_case(out, "edge", 0.5, -100);
        emit_case(out, "edge", 0.5, -1);
        emit_case(out, "edge", 0.5, 0);
        emit_case(out, "edge", 0.5, 1);
        emit_case(out, "edge", 0.5, 100);
        emit_case(out, "edge", 0.5, 1024);
        emit_case(out, "edge", 0.5, 1025);

        /* d == 1.0 -- exercises the rewrite-to-0.5 branch. exp gets
         * incremented internally, so exp = 1024 here corresponds to
         * the d = 0.5, exp = 1025 path. */
        emit_case(out, "edge", 1.0, -1074);  /* internal: d=0.5, exp=-1073 */
        emit_case(out, "edge", 1.0, -1023);  /* internal: d=0.5, exp=-1022 */
        emit_case(out, "edge", 1.0, 0);      /* internal: d=0.5, exp=1 */
        emit_case(out, "edge", 1.0, 1023);   /* internal: d=0.5, exp=1024 */
        emit_case(out, "edge", 1.0, 1024);   /* internal: d=0.5, exp=1025 */

        /* d just below 1.0 (0.9999...). Highest representable d strictly
         * less than 1 is nextDown(1.0). */
        emit_case(out, "edge", nextafter(1.0, 0.5), -1073);
        emit_case(out, "edge", nextafter(1.0, 0.5), -1022);
        emit_case(out, "edge", nextafter(1.0, 0.5), -1021);
        emit_case(out, "edge", nextafter(1.0, 0.5), 0);
        emit_case(out, "edge", nextafter(1.0, 0.5), 1024);

        /* Various d in [0.5, 1) with diverse mantissa patterns. */
        emit_case(out, "edge", 0.5, -500);
        emit_case(out, "edge", 0.5, 500);
        emit_case(out, "edge", 0.5625, 0);                       /* 9/16 */
        emit_case(out, "edge", 0.5625, -1000);
        emit_case(out, "edge", 0.5625, 1000);
        emit_case(out, "edge", 0.6666666666666666, 0);           /* approx 2/3 */
        emit_case(out, "edge", 0.6666666666666666, -1022);       /* subnormal-target */
        emit_case(out, "edge", 0.6666666666666666, -1021);       /* normal-target */
        emit_case(out, "edge", 0.6666666666666666, 1023);
        emit_case(out, "edge", 0.9999999999999999, 0);
        emit_case(out, "edge", 0.9999999999999999, -1073);
    }

    /* ============================================================== */
    /* adversarial: 10 -- subnormal-target boundary stress and exp    */
    /* extremes near the IEEE-754 representable limits.               */
    /* ============================================================== */
    {
        /* Walk through the subnormal boundary: exp = -1022 (last normal),
         * -1023, ..., -1073 (smallest representable subnormal output).
         * Test every step in the first 5 to be sure the DBL_EPSILON
         * path is correct. */
        emit_case(out, "adversarial", 0.75, -1022);
        emit_case(out, "adversarial", 0.75, -1023);
        emit_case(out, "adversarial", 0.75, -1024);
        emit_case(out, "adversarial", 0.75, -1025);
        emit_case(out, "adversarial", 0.75, -1073);

        /* exp at the upper extreme. */
        emit_case(out, "adversarial", 0.5, 1023);
        emit_case(out, "adversarial", nextafter(1.0, 0.5), 1024);
        emit_case(out, "adversarial", nextafter(1.0, 0.5), 1025);

        /* Edge of the rewrite-from-d=1 path at the boundaries. */
        emit_case(out, "adversarial", 1.0, -1023);  /* boundary subnormal */
        emit_case(out, "adversarial", 1.0, 1024);   /* boundary +max */
    }

    /* ============================================================== */
    /* fuzz: 50 -- xorshift-driven (d, exp) within the precondition   */
    /* envelope.                                                       */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xDEC0DEBA51FACADEULL);
        for (int i = 0; i < 50; ++i) {
            /* Random d in [0.5, 1.0): build from mantissa bits. */
            const uint64_t r1 = xs64_next(&rng);
            /* IEEE-754 binary64 in [0.5, 1.0): biased exp = 1022
             * (unbiased = -1), random 52-bit mantissa. */
            union { uint64_t u; double d; } u;
            u.u = (1022ULL << 52) | (r1 & ((1ULL << 52) - 1));
            const double d = u.d;
            /* Random exp in [-1073, 1025]. Range size = 2099. */
            const uint64_t r2 = xs64_next(&rng);
            const int exp = (int)((int64_t)(r2 % 2099) - 1073);
            emit_case(out, "fuzz", d, exp);
        }
    }

    return 0;
}
