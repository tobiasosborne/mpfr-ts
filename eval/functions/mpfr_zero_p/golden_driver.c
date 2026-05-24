/*
 * golden_driver.c — Golden master for MPFR's mpfr_zero_p.
 *
 * C signature: int mpfr_zero_p(mpfr_srcptr x). mpfr/src/iszero.c.
 * Sign-agnostic.
 *
 * Wire / tags identical to sibling predicates (nan_p, inf_p, number_p,
 * signbit). The broken zero_p we ship is `kind === 'normal'`, so the
 * adversarial bucket is heavy on normal inputs (every one flips).
 *
 * Ref: eval/functions/mpfr_nan_p/golden_driver.c — full rationale.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_zero_p golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    const uint64_t t0 = now_ns();
    const int raw = mpfr_zero_p(x);
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

    /* happy: ~25 — both signed zeros prominently, finite normals,
     * a few non-zero singularities. */
    {
        EMIT("happy", init_pos_zero(_x, 53));
        EMIT("happy", init_neg_zero(_x, 53));
        EMIT("happy", init_pos_zero(_x, 100));
        EMIT("happy", init_neg_zero(_x, 100));
        EMIT("happy", init_pos_zero(_x, 1));
        EMIT("happy", init_neg_zero(_x, 1));

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

        EMIT("happy", init_pos_inf(_x, 53));
        EMIT("happy", init_neg_inf(_x, 53));
        EMIT("happy", init_nan(_x, 53));

        EMIT("happy", init_from_double(_x, 1.0, 24));
        EMIT("happy", init_from_double(_x, 1.0, 64));
        EMIT("happy", init_from_double(_x, 1.0, 128));
        EMIT("happy", init_from_double(_x, 1.0, 200));
        EMIT("happy", init_from_double(_x, 1.0, 512));
    }

    /* edge: ~35 — signed zero at every precision sample. */
    {
        EMIT("edge", init_pos_zero(_x, 1));
        EMIT("edge", init_pos_zero(_x, 2));
        EMIT("edge", init_pos_zero(_x, 53));
        EMIT("edge", init_pos_zero(_x, 64));
        EMIT("edge", init_pos_zero(_x, 100));
        EMIT("edge", init_pos_zero(_x, 256));
        EMIT("edge", init_pos_zero(_x, 1024));
        EMIT("edge", init_neg_zero(_x, 1));
        EMIT("edge", init_neg_zero(_x, 2));
        EMIT("edge", init_neg_zero(_x, 53));
        EMIT("edge", init_neg_zero(_x, 64));
        EMIT("edge", init_neg_zero(_x, 100));
        EMIT("edge", init_neg_zero(_x, 256));
        EMIT("edge", init_neg_zero(_x, 1024));

        /* +0 produced via 0.0 set_d (signed zero round-trip). */
        EMIT("edge", init_from_double(_x, 0.0, 53));
        EMIT("edge", init_from_double(_x, 0.0, 100));

        /* -0 via memcpy of the bit pattern then set_d. */
        { mpfr_t _x; mpfr_init2(_x, 53);
          uint64_t bits = (uint64_t)1 << 63;
          double d; memcpy(&d, &bits, sizeof d);
          mpfr_set_d(_x, d, MPFR_RNDN);
          emit_case(out, "edge", _x); mpfr_clear(_x); }

        EMIT("edge", init_nan(_x, 1));
        EMIT("edge", init_nan(_x, 53));
        EMIT("edge", init_nan(_x, 100));
        EMIT("edge", init_nan(_x, 256));
        EMIT("edge", init_pos_inf(_x, 1));
        EMIT("edge", init_pos_inf(_x, 53));
        EMIT("edge", init_pos_inf(_x, 256));
        EMIT("edge", init_neg_inf(_x, 53));

        EMIT("edge", init_from_double(_x, 1.0, 1));
        EMIT("edge", init_from_double(_x, -1.0, 1));
        EMIT("edge", init_from_double(_x, 0.5, 1));
        EMIT("edge", init_from_double(_x, 5e-324, 53));
        EMIT("edge", init_from_double(_x, 1.7e308, 53));
        EMIT("edge", init_from_double(_x, 2.0, 53));
        EMIT("edge", init_from_double(_x, 1024.0, 53));
        EMIT("edge", init_from_double(_x, 1.0/1024.0, 53));
        EMIT("edge", init_from_double(_x, 1.0, 4096));
    }

    /* adversarial: ~15 — heavy 'normal'-kind representation (broken
     * zero_p is `kind === 'normal'` → flips on all of these). */
    {
        EMIT("adversarial", init_from_double(_x, 1.0, 53));
        EMIT("adversarial", init_from_double(_x, 2.0, 53));
        EMIT("adversarial", init_from_double(_x, 1.5, 53));
        EMIT("adversarial", init_from_double(_x, -1.5, 53));
        EMIT("adversarial", init_from_double(_x, 0.5, 53));
        EMIT("adversarial", init_from_double(_x, -0.5, 53));
        EMIT("adversarial", init_from_double(_x, 1.0, 100));
        EMIT("adversarial", init_from_double(_x, 1.0, 256));
        EMIT("adversarial", init_from_double(_x, 1.0, 1));
        EMIT("adversarial", init_from_double(_x, 1.0, 1024));

        /* And the true cases — zero in multiple flavours. */
        EMIT("adversarial", init_pos_zero(_x, 1));
        EMIT("adversarial", init_pos_zero(_x, 53));
        EMIT("adversarial", init_neg_zero(_x, 1));
        EMIT("adversarial", init_neg_zero(_x, 53));

        /* Mix-in: NaN and Inf which neither correct nor broken include. */
        EMIT("adversarial", init_nan(_x, 53));
    }

    /* fuzz: 60 — identical generator to nan_p / inf_p (shared seed). */
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

    /* mined: 5 — tisnan.c style. */
    {
        EMIT("mined", init_pos_zero(_x, 53));   /* set_zero +1 → true */
        EMIT("mined", init_neg_zero(_x, 53));   /* set_zero -1 → true */
        EMIT("mined", init_nan(_x, 53));        /* set_nan → false */
        EMIT("mined", init_pos_inf(_x, 53));    /* set_inf +1 → false */
        EMIT("mined", init_from_double(_x, 1.0, 53)); /* finite → false */
    }

    return 0;
}
