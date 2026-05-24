/*
 * golden_driver.c — Golden master for MPFR's mpfr_signbit.
 *
 * C signature: int mpfr_signbit(mpfr_srcptr x). mpfr/src/signbit.c.
 * Defined for every kind: returns true iff `_mpfr_sign < 0`. Includes
 * -0 and -Inf as true (signed zero / signed inf signbit is observable
 * — this is the predicate the manual recommends for distinguishing
 * +0 from -0, which mpfr_zero_p collapses).
 *
 * Critical: NaN. libmpfr's NaN can carry either sign in principle, but
 * mpfr_set_nan initialises it sign-positive. The golden generated here
 * therefore agrees with the TS port (which canonicalises NaN to
 * sign = 1, signbit = false) on every NaN case — see spec.json
 * divergence note.
 *
 * Wire / tags identical to sibling predicates. The broken signbit we
 * ship is the polarity flip (`sign === 1`), so every case flips.
 *
 * Ref: eval/functions/mpfr_nan_p/golden_driver.c — full rationale.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_signbit golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    const uint64_t t0 = now_ns();
    const int raw = mpfr_signbit(x);
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

    /* happy: ~25 — balanced positive/negative, including signed zeros
     * and signed infinities to exercise the "signbit looks at sign,
     * not at kind" invariant. */
    {
        EMIT("happy", init_from_double(_x, 1.0, 53));
        EMIT("happy", init_from_double(_x, -1.0, 53));
        EMIT("happy", init_from_double(_x, 3.14, 53));
        EMIT("happy", init_from_double(_x, -3.14, 53));
        EMIT("happy", init_from_double(_x, 1.5e100, 53));
        EMIT("happy", init_from_double(_x, -1.5e100, 53));
        EMIT("happy", init_from_double(_x, 42.0, 53));
        EMIT("happy", init_from_double(_x, -42.0, 53));
        EMIT("happy", init_from_double(_x, 0.5, 53));
        EMIT("happy", init_from_double(_x, -0.5, 53));

        EMIT("happy", init_pos_zero(_x, 53));
        EMIT("happy", init_neg_zero(_x, 53));

        EMIT("happy", init_pos_inf(_x, 53));
        EMIT("happy", init_neg_inf(_x, 53));

        EMIT("happy", init_nan(_x, 53));
        EMIT("happy", init_nan(_x, 100));

        EMIT("happy", init_from_double(_x, 1.0, 24));
        EMIT("happy", init_from_double(_x, -1.0, 24));
        EMIT("happy", init_from_double(_x, 1.0, 64));
        EMIT("happy", init_from_double(_x, -1.0, 64));
        EMIT("happy", init_from_double(_x, 1.0, 128));
        EMIT("happy", init_from_double(_x, -1.0, 128));
        EMIT("happy", init_from_double(_x, 1.0, 200));
        EMIT("happy", init_from_double(_x, -1.0, 200));
        EMIT("happy", init_from_double(_x, 1.0, 512));
    }

    /* edge: ~35 — emphasise signed singularities at many precs. */
    {
        EMIT("edge", init_pos_zero(_x, 1));
        EMIT("edge", init_pos_zero(_x, 53));
        EMIT("edge", init_pos_zero(_x, 100));
        EMIT("edge", init_pos_zero(_x, 256));
        EMIT("edge", init_pos_zero(_x, 1024));
        EMIT("edge", init_neg_zero(_x, 1));
        EMIT("edge", init_neg_zero(_x, 53));
        EMIT("edge", init_neg_zero(_x, 100));
        EMIT("edge", init_neg_zero(_x, 256));
        EMIT("edge", init_neg_zero(_x, 1024));

        EMIT("edge", init_pos_inf(_x, 1));
        EMIT("edge", init_pos_inf(_x, 53));
        EMIT("edge", init_pos_inf(_x, 100));
        EMIT("edge", init_pos_inf(_x, 256));
        EMIT("edge", init_neg_inf(_x, 1));
        EMIT("edge", init_neg_inf(_x, 53));
        EMIT("edge", init_neg_inf(_x, 100));
        EMIT("edge", init_neg_inf(_x, 256));

        /* NaN at various precs — all should still emit canonical
         * TS NaN (sign=1) → signbit false. */
        EMIT("edge", init_nan(_x, 1));
        EMIT("edge", init_nan(_x, 53));
        EMIT("edge", init_nan(_x, 100));
        EMIT("edge", init_nan(_x, 256));
        EMIT("edge", init_nan(_x, 1000));

        /* Negative -0 via memcpy round-trip. */
        { mpfr_t _x; mpfr_init2(_x, 53);
          uint64_t bits = (uint64_t)1 << 63;
          double d; memcpy(&d, &bits, sizeof d);
          mpfr_set_d(_x, d, MPFR_RNDN);
          emit_case(out, "edge", _x); mpfr_clear(_x); }

        EMIT("edge", init_from_double(_x, 1.0, 1));
        EMIT("edge", init_from_double(_x, -1.0, 1));
        EMIT("edge", init_from_double(_x, 5e-324, 53));
        EMIT("edge", init_from_double(_x, -5e-324, 53));
        EMIT("edge", init_from_double(_x, 1.7e308, 53));
        EMIT("edge", init_from_double(_x, -1.7e308, 53));
        EMIT("edge", init_from_double(_x, 2.0, 53));
        EMIT("edge", init_from_double(_x, -2.0, 53));
        EMIT("edge", init_from_double(_x, 1024.0, 53));
        EMIT("edge", init_from_double(_x, -1024.0, 53));
        EMIT("edge", init_from_double(_x, 1.0, 4096));
    }

    /* adversarial: ~15 — both signs at non-default precs; broken sign-flip
     * is total so every case flips regardless. */
    {
        EMIT("adversarial", init_from_double(_x, 1.0, 1));
        EMIT("adversarial", init_from_double(_x, -1.0, 1));
        EMIT("adversarial", init_from_double(_x, 1.0, 100));
        EMIT("adversarial", init_from_double(_x, -1.0, 100));
        EMIT("adversarial", init_pos_zero(_x, 200));
        EMIT("adversarial", init_neg_zero(_x, 200));
        EMIT("adversarial", init_pos_inf(_x, 200));
        EMIT("adversarial", init_neg_inf(_x, 200));
        EMIT("adversarial", init_nan(_x, 200));

        { mpfr_t _x; mpfr_init2(_x, 53);
          mpfr_set_str(_x, "1.111111111111111111111111111111111111111111111111111E1023", 2, MPFR_RNDN);
          emit_case(out, "adversarial", _x); mpfr_clear(_x); }
        { mpfr_t _x; mpfr_init2(_x, 53);
          mpfr_set_str(_x, "-1.111111111111111111111111111111111111111111111111111E1023", 2, MPFR_RNDN);
          emit_case(out, "adversarial", _x); mpfr_clear(_x); }
        { mpfr_t _x; mpfr_init2(_x, 53);
          mpfr_set_str(_x, "1.0E-1022", 2, MPFR_RNDN);
          emit_case(out, "adversarial", _x); mpfr_clear(_x); }
        { mpfr_t _x; mpfr_init2(_x, 53);
          mpfr_set_str(_x, "-1.0E-1022", 2, MPFR_RNDN);
          emit_case(out, "adversarial", _x); mpfr_clear(_x); }

        /* set_si negative & positive. */
        { mpfr_t _x; mpfr_init2(_x, 64); mpfr_set_si(_x, -12345, MPFR_RNDN);
          emit_case(out, "adversarial", _x); mpfr_clear(_x); }
        { mpfr_t _x; mpfr_init2(_x, 64); mpfr_set_si(_x, 12345, MPFR_RNDN);
          emit_case(out, "adversarial", _x); mpfr_clear(_x); }
    }

    /* fuzz: 60 — same shared mix; the random double's sign bit drives
     * the answer for the (dominant) normal-kind cases. */
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

    /* mined: 5 — tsignbit.c style assertions. */
    {
        EMIT("mined", init_pos_zero(_x, 53));   /* +0 → false */
        EMIT("mined", init_neg_zero(_x, 53));   /* -0 → true */
        EMIT("mined", init_pos_inf(_x, 53));    /* +Inf → false */
        EMIT("mined", init_neg_inf(_x, 53));    /* -Inf → true */
        EMIT("mined", init_from_double(_x, -1.0, 53));  /* -1.0 → true */
    }

    return 0;
}
