/*
 * golden_driver.c — Golden master for MPFR's mpfr_nan_p.
 *
 * C signature
 * -----------
 *
 *   int mpfr_nan_p(mpfr_srcptr x);
 *
 *   Returns non-zero iff x is NaN, zero otherwise. mpfr/src/isnan.c
 *   L24–L28:
 *
 *     int (mpfr_nan_p) (mpfr_srcptr x) { return MPFR_IS_NAN(x); }
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The TS port `mpfr_nan_p(x) -> boolean` takes one immutable MPFR
 * struct and returns a plain boolean. Never throws — every kind has a
 * well-defined yes/no answer.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":{<MPFR>}},
 *    "output":<bool>,
 *    "time_ns":<n>}
 *
 *   `output` is a bare JSON boolean emitted by jl_output_scalar_bool.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25
 *   edge         :  ~35
 *   adversarial  :  ~15
 *   fuzz         :   60
 *   mined        :    5
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/isnan.c — the C reference.
 * Ref: src/ops/nan_p.ts — the production port.
 * Ref: mpfr/tests/tisnan.c — source for `mined` cases.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_nan_p golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Emit one mpfr_nan_p case. */
static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    const uint64_t t0 = now_ns();
    const int raw = mpfr_nan_p(x);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    jl_output_scalar_bool(out, raw);
    jl_finish(out, elapsed);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_pos_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1); }
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1); }
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1); }
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1); }
static inline void init_nan(mpfr_ptr x, uint64_t prec)      { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_nan(x); }

#define EMIT(tag, initexpr) do { \
    mpfr_t _x; initexpr; emit_case(out, tag, _x); mpfr_clear(_x); \
} while (0)

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: ~25 cases — most are non-NaN finite/inf, a few NaN.     */
    /* The mix matches the runtime distribution callers see: NaN is   */
    /* rare. A 4:1 non-NaN-to-NaN ratio here exercises the common-    */
    /* false path well.                                                */
    /* ============================================================== */
    {
        EMIT("happy", init_from_double(_x, 1.0, 53));
        EMIT("happy", init_from_double(_x, -1.0, 53));
        EMIT("happy", init_from_double(_x, 3.14, 53));
        EMIT("happy", init_from_double(_x, -2.718, 53));
        EMIT("happy", init_from_double(_x, 1.5e100, 53));
        EMIT("happy", init_from_double(_x, -1.5e-100, 53));
        EMIT("happy", init_from_double(_x, 42.0, 53));
        EMIT("happy", init_from_double(_x, 0.5, 53));
        EMIT("happy", init_from_double(_x, 0.25, 53));
        EMIT("happy", init_from_double(_x, 6.022e23, 53));

        EMIT("happy", init_pos_zero(_x, 53));
        EMIT("happy", init_neg_zero(_x, 53));
        EMIT("happy", init_pos_inf(_x, 53));
        EMIT("happy", init_neg_inf(_x, 53));

        EMIT("happy", init_nan(_x, 53));
        EMIT("happy", init_nan(_x, 53));
        EMIT("happy", init_nan(_x, 53));
        EMIT("happy", init_nan(_x, 53));

        /* Various precs (still non-NaN). */
        EMIT("happy", init_from_double(_x, 1.0, 24));
        EMIT("happy", init_from_double(_x, 1.0, 64));
        EMIT("happy", init_from_double(_x, 1.0, 100));
        EMIT("happy", init_from_double(_x, 1.0, 128));
        EMIT("happy", init_from_double(_x, 1.0, 200));
        EMIT("happy", init_from_double(_x, 1.0, 256));
        EMIT("happy", init_from_double(_x, 1.0, 512));
    }

    /* ============================================================== */
    /* edge: ~35 cases — kind × prec sweep, signed singularities,    */
    /* prec=1 boundary, very large prec, NaN at multiple precs (which */
    /* should ALL still emit canonical TS NaN with prec=0).           */
    /* ============================================================== */
    {
        /* Each kind at multiple precs. NaN at varying input prec stress-
         * tests that the wire encoding canonicalises NaN to prec=0 on the
         * TS side (jl_kv_mpfr handles this). */
        EMIT("edge", init_nan(_x, 1));
        EMIT("edge", init_nan(_x, 2));
        EMIT("edge", init_nan(_x, 53));
        EMIT("edge", init_nan(_x, 64));
        EMIT("edge", init_nan(_x, 100));
        EMIT("edge", init_nan(_x, 256));
        EMIT("edge", init_nan(_x, 1000));

        EMIT("edge", init_pos_inf(_x, 1));
        EMIT("edge", init_pos_inf(_x, 53));
        EMIT("edge", init_pos_inf(_x, 256));
        EMIT("edge", init_neg_inf(_x, 1));
        EMIT("edge", init_neg_inf(_x, 53));
        EMIT("edge", init_neg_inf(_x, 256));

        EMIT("edge", init_pos_zero(_x, 1));
        EMIT("edge", init_pos_zero(_x, 53));
        EMIT("edge", init_pos_zero(_x, 256));
        EMIT("edge", init_neg_zero(_x, 1));
        EMIT("edge", init_neg_zero(_x, 53));
        EMIT("edge", init_neg_zero(_x, 256));

        /* prec=1 normals, both signs. */
        EMIT("edge", init_from_double(_x, 1.0, 1));
        EMIT("edge", init_from_double(_x, -1.0, 1));
        EMIT("edge", init_from_double(_x, 0.5, 1));
        EMIT("edge", init_from_double(_x, -0.5, 1));

        /* Very large prec normals. */
        EMIT("edge", init_from_double(_x, 1.0, 1024));
        EMIT("edge", init_from_double(_x, 1.0, 4096));

        /* Tiny / huge magnitudes. */
        EMIT("edge", init_from_double(_x, 5e-324, 53));   /* min subnormal-ish double */
        EMIT("edge", init_from_double(_x, 1.7e308, 53));  /* near DBL_MAX */
        EMIT("edge", init_from_double(_x, -5e-324, 53));
        EMIT("edge", init_from_double(_x, -1.7e308, 53));

        /* Powers of two — clean mantissa, exercises kind discriminant. */
        EMIT("edge", init_from_double(_x, 2.0, 53));
        EMIT("edge", init_from_double(_x, 4.0, 53));
        EMIT("edge", init_from_double(_x, 1024.0, 53));
        EMIT("edge", init_from_double(_x, 1.0/1024.0, 53));

        /* NaN constructed via 0/0 produced by mpfr_div. Should be NaN. */
        { mpfr_t a, b, _x; mpfr_init2(a, 53); mpfr_init2(b, 53); mpfr_init2(_x, 53);
          mpfr_set_zero(a, 1); mpfr_set_zero(b, 1);
          mpfr_div(_x, a, b, MPFR_RNDN);
          emit_case(out, "edge", _x);
          mpfr_clear(a); mpfr_clear(b); mpfr_clear(_x); }

        /* NaN via Inf - Inf. */
        { mpfr_t a, b, _x; mpfr_init2(a, 53); mpfr_init2(b, 53); mpfr_init2(_x, 53);
          mpfr_set_inf(a, 1); mpfr_set_inf(b, 1);
          mpfr_sub(_x, a, b, MPFR_RNDN);
          emit_case(out, "edge", _x);
          mpfr_clear(a); mpfr_clear(b); mpfr_clear(_x); }
    }

    /* ============================================================== */
    /* adversarial: ~15 — kind-flip traps. The broken nan_p we ship is */
    /* `kind === 'inf'`, so we lean on Inf cases to make the gap stark. */
    /* ============================================================== */
    {
        /* Many Inf inputs — broken nan_p returns true on these (wrong). */
        EMIT("adversarial", init_pos_inf(_x, 53));
        EMIT("adversarial", init_neg_inf(_x, 53));
        EMIT("adversarial", init_pos_inf(_x, 100));
        EMIT("adversarial", init_neg_inf(_x, 100));
        EMIT("adversarial", init_pos_inf(_x, 200));
        EMIT("adversarial", init_neg_inf(_x, 200));
        EMIT("adversarial", init_pos_inf(_x, 1));
        EMIT("adversarial", init_neg_inf(_x, 1));

        /* And many NaN inputs — broken nan_p returns false on these. */
        EMIT("adversarial", init_nan(_x, 53));
        EMIT("adversarial", init_nan(_x, 100));
        EMIT("adversarial", init_nan(_x, 1));
        EMIT("adversarial", init_nan(_x, 200));

        /* Mantissa-near-MSB normals to break a kind-mistaken implementation. */
        { mpfr_t _x; mpfr_init2(_x, 53);
          mpfr_set_str(_x, "1.111111111111111111111111111111111111111111111111111E1023", 2, MPFR_RNDN);
          emit_case(out, "adversarial", _x); mpfr_clear(_x); }
        { mpfr_t _x; mpfr_init2(_x, 53);
          mpfr_set_str(_x, "1.0E-1022", 2, MPFR_RNDN);
          emit_case(out, "adversarial", _x); mpfr_clear(_x); }

        /* Signed zero at exotic precs — neither broken kind matches. */
        EMIT("adversarial", init_pos_zero(_x, 1));
        EMIT("adversarial", init_neg_zero(_x, 1));
    }

    /* ============================================================== */
    /* fuzz: 60 — PRNG-driven; mix of finite normals, signed zeros,   */
    /* infinities, and NaN. The seed (0xC0FFEEC0FFEEC0FF) matches      */
    /* sibling predicate fuzz seeds for cross-port reproducibility.    */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEEC0FFEEC0FFULL);
        const uint64_t precs[6] = { 1, 53, 64, 100, 128, 256 };

        int emitted = 0;
        while (emitted < 60) {
            const uint64_t r = xs64_next(&rng);
            const uint64_t kind_choice = r % 10;
            const uint64_t prec = precs[xs64_below(&rng, 6)];

            mpfr_t _x;
            mpfr_init2(_x, (mpfr_prec_t)prec);

            if (kind_choice == 0) {
                mpfr_set_nan(_x);
            } else if (kind_choice == 1) {
                mpfr_set_inf(_x, (r & (1ULL << 32)) ? -1 : 1);
            } else if (kind_choice == 2) {
                mpfr_set_zero(_x, (r & (1ULL << 32)) ? -1 : 1);
            } else {
                /* finite normal from a random double, skipping the
                 * IEEE specials so set_d gives us a clean normal. */
                uint64_t bits;
                do { bits = xs64_next(&rng); } while (((bits >> 52) & 0x7FF) == 0x7FF);
                double d;
                memcpy(&d, &bits, sizeof d);
                mpfr_set_d(_x, d, MPFR_RNDN);
            }

            emit_case(out, "fuzz", _x);
            mpfr_clear(_x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 — transcribed from mpfr/tests/tisnan.c which exercises */
    /* every predicate-of-singular-value invariant.                    */
    /* ============================================================== */
    {
        /* tisnan.c: mpfr_set_nan(x); ASSERT(mpfr_nan_p(x)); */
        EMIT("mined", init_nan(_x, 53));

        /* tisnan.c: mpfr_set_inf(x, +1); ASSERT(!mpfr_nan_p(x)); */
        EMIT("mined", init_pos_inf(_x, 53));

        /* tisnan.c: mpfr_set_inf(x, -1); ASSERT(!mpfr_nan_p(x)); */
        EMIT("mined", init_neg_inf(_x, 53));

        /* tisnan.c: mpfr_set_zero(x, +1); ASSERT(!mpfr_nan_p(x)); */
        EMIT("mined", init_pos_zero(_x, 53));

        /* tisnan.c: a finite normal ASSERT(!mpfr_nan_p(x)); */
        EMIT("mined", init_from_double(_x, 1.0, 53));
    }

    return 0;
}
