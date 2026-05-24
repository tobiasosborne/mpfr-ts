/*
 * golden_driver.c — Golden master for MPFR's mpfr_set_exp.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_exp(mpfr_ptr x, mpfr_exp_t exponent);
 *
 * Mutates x.exp to `exponent` if PURE_FP and e ∈ [emin, emax]; returns
 * 0 on success, 1 on failure. Ref: mpfr/src/set_exp.c L24-L38.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS port `mpfr_set_exp(x, e) -> MPFR` returns a fresh MPFR with the
 * new exp on success; throws MPFRError('EDOMAIN') on non-normal input
 * or e out of range.
 *
 * Since the TS port throws on bad inputs (counted as non-pass), the
 * golden emits ONLY normal inputs with `e` within the safe range
 * [-(2^30 - 1) + 1000, (2^30 - 1) - 1000]. Each case:
 *
 *   1. Build a normal mpfr_t with its own exp.
 *   2. Call mpfr_set_exp(x, e). Assert returns 0 (i.e. e is in-range).
 *   3. Emit the resulting MPFR (which has the new exp).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-normal>,"e":"<decimal exp>"},
 *    "output":<MPFR-normal>,
 *    "time_ns":<n>}
 *
 *   - x is a normal MPFR.
 *   - e is a decimal-string bigint.
 *   - output is a bare MPFR (jl_output_mpfr): {kind:'normal', sign:x.sign,
 *     prec:x.prec, exp:e, mant:x.mant}.
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  22
 *   edge         :  30
 *   adversarial  :  10
 *   fuzz         :  55
 *   mined        :   5
 *
 * Ref: mpfr/src/set_exp.c — C reference.
 * Ref: src/ops/set_exp.ts — production port (to be written by sonnet).
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_exp golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Safe range: ±(EMAX_DEFAULT - 1000), staying clear of the boundary. */
#define SAFE_EXP_MAX ((int64_t)(((1LL << 30) - 1) - 1000))
#define SAFE_EXP_MIN ((int64_t)(-SAFE_EXP_MAX))

static void emit_case(FILE *out, const char *tag,
                      mpfr_srcptr x_in, int64_t e) {
    assert(mpfr_regular_p(x_in));  /* only normal inputs */
    assert(e >= SAFE_EXP_MIN && e <= SAFE_EXP_MAX);

    /* Clone x_in (mpfr_set_exp mutates). */
    mpfr_t x;
    mpfr_init2(x, mpfr_get_prec(x_in));
    mpfr_set(x, x_in, MPFR_RNDN);

    const uint64_t t0 = now_ns();
    const int rc = mpfr_set_exp(x, (mpfr_exp_t)e);
    const uint64_t elapsed = now_ns() - t0;
    assert(rc == 0);  /* e is in range and x is PURE_FP */
    (void)rc;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x_in);
    jl_kv_i64(out, 0, "e", e);
    jl_end_inputs(out);
    jl_output_mpfr(out, x);
    jl_finish(out, elapsed);

    mpfr_clear(x);
}

static void mk_norm_d(mpfr_ptr x, mpfr_prec_t prec, double d) {
    mpfr_init2(x, prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}

static void mk_norm_si(mpfr_ptr x, mpfr_prec_t prec, long v) {
    mpfr_init2(x, prec);
    mpfr_set_si(x, v, MPFR_RNDN);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 — common values × common exps. */
    {
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "happy", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "happy", x, 10); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "happy", x, -10); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 3.14); emit_case(out, "happy", x, 100); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 3.14); emit_case(out, "happy", x, -100); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 2.71); emit_case(out, "happy", x, 50); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, -3.14); emit_case(out, "happy", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, -3.14); emit_case(out, "happy", x, 100); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, -2.71); emit_case(out, "happy", x, -50); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 53, 7); emit_case(out, "happy", x, 5); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 53, 7); emit_case(out, "happy", x, -5); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 53, -7); emit_case(out, "happy", x, 5); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 24, 1.5); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 24, 1.5); emit_case(out, "happy", x, 100); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 113, 1.0/3.0); emit_case(out, "happy", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 113, 1.0/3.0); emit_case(out, "happy", x, 50); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 113, -1.0/3.0); emit_case(out, "happy", x, -50); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 100, 1.0); emit_case(out, "happy", x, 1000); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 100, 1.0); emit_case(out, "happy", x, -1000); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 64, 1.5e10); emit_case(out, "happy", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 80, 1e-50); emit_case(out, "happy", x, 0); mpfr_clear(x); }
    }

    /* edge: 30 — exp boundaries, PREC_MIN, mantissa extremes. */
    {
        { mpfr_t x; mk_norm_d(x, 53, 1.5); emit_case(out, "edge", x, SAFE_EXP_MAX); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.5); emit_case(out, "edge", x, SAFE_EXP_MIN); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.5); emit_case(out, "edge", x, SAFE_EXP_MAX - 1); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.5); emit_case(out, "edge", x, SAFE_EXP_MIN + 1); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, -1.5); emit_case(out, "edge", x, SAFE_EXP_MAX); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, -1.5); emit_case(out, "edge", x, SAFE_EXP_MIN); mpfr_clear(x); }
        /* PREC_MIN. */
        { mpfr_t x; mk_norm_si(x, 1, 1); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 1, 1); emit_case(out, "edge", x, 100); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 1, 1); emit_case(out, "edge", x, -100); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 1, -1); emit_case(out, "edge", x, SAFE_EXP_MAX); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 1, -1); emit_case(out, "edge", x, SAFE_EXP_MIN); mpfr_clear(x); }
        /* Limb-boundary precs. */
        { mpfr_t x; mk_norm_d(x, 63, 1.5); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 64, 1.5); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 65, 1.5); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 127, 1.5); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 128, 1.5); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 129, 1.5); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        /* exp = 0 boundary. */
        { mpfr_t x; mk_norm_d(x, 53, 100.0); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 0.001); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        /* Large prec. */
        { mpfr_t x; mk_norm_d(x, 1024, 1.5); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 1024, 1.5); emit_case(out, "edge", x, 1000); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 2048, 1.5); emit_case(out, "edge", x, -1000); mpfr_clear(x); }
        /* Mantissa near upper limit. */
        { mpfr_t x; mk_norm_d(x, 53, 1.7976931348623157e308); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        /* Mantissa near lower limit. */
        { mpfr_t x; mk_norm_d(x, 53, 5e-324); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        /* Various small exps. */
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "edge", x, -1); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "edge", x, 2); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "edge", x, -2); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 4, 7); emit_case(out, "edge", x, 100); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 4, 7); emit_case(out, "edge", x, -100); mpfr_clear(x); }
    }

    /* adversarial: 10 — mantissa+exp boundaries together. */
    {
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "adversarial", x, SAFE_EXP_MAX); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "adversarial", x, SAFE_EXP_MIN); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, -1.0); emit_case(out, "adversarial", x, SAFE_EXP_MAX); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, -1.0); emit_case(out, "adversarial", x, SAFE_EXP_MIN); mpfr_clear(x); }
        /* Carry-on-rounding mantissa, then set_exp. */
        { mpfr_t x; mk_norm_d(x, 53, 0.9999999999999999); emit_case(out, "adversarial", x, 50); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.0000000000000002); emit_case(out, "adversarial", x, 50); mpfr_clear(x); }
        /* Tiny double. */
        { mpfr_t x; mk_norm_d(x, 53, 5e-324); emit_case(out, "adversarial", x, 50); mpfr_clear(x); }
        /* Huge double. */
        { mpfr_t x; mk_norm_d(x, 53, 1.7976931348623157e308); emit_case(out, "adversarial", x, -50); mpfr_clear(x); }
        /* Large prec + small exp. */
        { mpfr_t x; mk_norm_d(x, 1024, 1.5); emit_case(out, "adversarial", x, 1); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 1024, 1.5); emit_case(out, "adversarial", x, -1); mpfr_clear(x); }
    }

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5E5E73AC471FACEULL);
        int emitted = 0;
        while (emitted < 55) {
            const uint64_t prec = 1 + xs64_below(&rng, 200);
            const double mag = (double)(1 + xs64_below(&rng, 1000000));
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            mpfr_t x;
            mpfr_init2(x, (mpfr_prec_t)prec);
            mpfr_set_d(x, sign * mag, MPFR_RNDN);
            if (!mpfr_regular_p(x)) {
                mpfr_clear(x);
                continue;
            }
            /* Random e in [SAFE_EXP_MIN, SAFE_EXP_MAX] via two RNG draws. */
            const uint64_t r1 = xs64_next(&rng);
            const int64_t e = (int64_t)(r1 % (uint64_t)(2 * SAFE_EXP_MAX + 1)) - SAFE_EXP_MAX;
            emit_case(out, "fuzz", x, e);
            emitted++;
            mpfr_clear(x);
        }
    }

    /* mined: 5 — mpfr/tests/tset_exp.c patterns. */
    {
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "mined", x, 0); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "mined", x, 1); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "mined", x, -1); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 3.14); emit_case(out, "mined", x, 10); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 113, 1.0/3.0); emit_case(out, "mined", x, 100); mpfr_clear(x); }
    }

    return 0;
}
