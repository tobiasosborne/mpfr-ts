/*
 * golden_driver.c -- Golden master for MPFR's mpfr_total_order_p.
 *
 * C: int mpfr_total_order_p (mpfr_srcptr x, mpfr_srcptr y)
 *    IEEE 754-2008 Section 5.10 totalOrder. True iff x <= y under
 *    -NaN < -Inf < negatives < -0 < +0 < positives < +Inf < +NaN.
 *    Ref: mpfr/src/total_order.c L26-L42.
 *
 * Wire: {"inputs":{"x":{<mpfr>},"y":{<mpfr>}},"output":<bool>}.
 *   output is a bare JSON boolean via jl_output_scalar_bool.
 *
 * Tag distribution (Rule 7): happy 25, edge 40, adv 16, fuzz 60, mined 5.
 *
 * CRUCIAL: only +NaN inputs are emitted. The locked TS schema cannot
 * represent a -NaN (every NaN folds to sign=1), so a -NaN golden case
 * would be an unfixable C-vs-TS divergence. libmpfr's mpfr_set_nan
 * yields a +NaN (signbit=0, verified). Signed ZEROS are fully exercised
 * since totalOrder orders them (-0 < +0) and they ARE representable.
 *
 * Ref: mpfr/src/total_order.c -- C reference.
 * Ref: eval/functions/mpfr_lessgreater_p/golden_driver.c -- structural
 *   template (predicate-with-boolean-output, kind x kind edge matrix).
 * Ref: src/ops/total_order_p.ts -- the production port (later).
 */
#include "common.h"

#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_total_order_p golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr a, mpfr_srcptr b) {
    const uint64_t t0 = now_ns();
    const int raw = mpfr_total_order_p(a, b);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", a);
    jl_kv_mpfr(out, 0, "y", b);
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
/* +NaN only (mpfr_set_nan produces signbit=0). -NaN is NOT emitted. */
static inline void init_nan(mpfr_ptr x, uint64_t prec)      { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_nan(x); }

static inline void emit_dd(FILE *out, const char *tag,
                           double da, uint64_t pa, double db, uint64_t pb) {
    mpfr_t a, b;
    init_from_double(a, da, pa);
    init_from_double(b, db, pb);
    emit_case(out, tag, a, b);
    mpfr_clear(a); mpfr_clear(b);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 25 -- typical strict <, >, == finite pairs.              */
    /* ============================================================== */
    {
        /* x < y -> true. */
        emit_dd(out, "happy",  1.0,  53,  2.0,  53);
        emit_dd(out, "happy",  0.0,  53,  1.0,  53);
        emit_dd(out, "happy", -1.0,  53,  1.0,  53);
        emit_dd(out, "happy", -10.0, 53, -5.0,  53);
        emit_dd(out, "happy", 1e-10, 53,  1.0,  53);
        emit_dd(out, "happy", 1e9,   53,  1e10, 53);
        emit_dd(out, "happy", -100.0, 53, 100.0, 53);
        emit_dd(out, "happy", 2.71828, 53, 3.14159, 53);

        /* x > y -> false. */
        emit_dd(out, "happy",  2.0,  53,  1.0,  53);
        emit_dd(out, "happy",  1.0,  53,  0.0,  53);
        emit_dd(out, "happy",  1.0,  53, -1.0,  53);
        emit_dd(out, "happy",  -5.0, 53, -10.0, 53);
        emit_dd(out, "happy",  1.0,  53, 1e-10, 53);
        emit_dd(out, "happy",  100.0, 53, 99.0, 53);
        emit_dd(out, "happy", 3.14159, 53, 2.71828, 53);

        /* x == y -> true (reflexive: x <= x). */
        emit_dd(out, "happy",  1.0,  53,  1.0,  53);
        emit_dd(out, "happy",  3.14, 53,  3.14, 53);
        emit_dd(out, "happy", -1.0,  53, -1.0,  53);
        emit_dd(out, "happy",  42.0, 53,  42.0, 53);
        emit_dd(out, "happy",  1e100, 53, 1e100, 53);

        /* equal-value different prec -> true both directions (lessequal). */
        emit_dd(out, "happy",  1.0,  53,  1.0,  200);
        emit_dd(out, "happy",  1.0, 200,  1.0,  53);
        emit_dd(out, "happy",  0.5,   2,  0.5,  53);
        emit_dd(out, "happy",  1.5,   2,  1.5,  100);
        emit_dd(out, "happy",  6.022e23, 53, 6.022e23, 53);
    }

    /* ============================================================== */
    /* edge: 40 -- signed zeros (ORDERED), +NaN extremes, +/-Inf.      */
    /* ============================================================== */
    {
        /* Signed zero -- totalOrder orders -0 < +0 (8). */
        { mpfr_t a, b; init_neg_zero(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* true */
        { mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* false */
        { mpfr_t a, b; init_pos_zero(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* true */
        { mpfr_t a, b; init_neg_zero(a, 53); init_neg_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* true */
        { mpfr_t a, b; init_neg_zero(a, 53); init_pos_zero(b, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* true, diff prec */
        { mpfr_t a, b; init_pos_zero(a, 200); init_neg_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* false, diff prec */
        { mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, 1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* -0 < 1 true */
        { mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, -1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* +0 > -1 false */

        /* Zero vs nonzero (6). */
        { mpfr_t a, b; init_from_double(a, -1.0, 53); init_neg_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* -1 < -0 true */
        { mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, -1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* -0 > -1 false */
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* 1 > +0 false */
        { mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, 1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* +0 < 1 true */
        { mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, -0.5, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* -0 > -0.5 false */
        { mpfr_t a, b; init_from_double(a, -0.5, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* -0.5 < +0 true */

        /* +/-Inf (10). */
        { mpfr_t a, b; init_neg_inf(a, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* -Inf < +Inf true */
        { mpfr_t a, b; init_pos_inf(a, 53); init_neg_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* false */
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* true reflexive */
        { mpfr_t a, b; init_neg_inf(a, 53); init_neg_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* true reflexive */
        { mpfr_t a, b; init_from_double(a, 1e100, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* finite < +Inf true */
        { mpfr_t a, b; init_pos_inf(a, 53); init_from_double(b, 1e100, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* +Inf > finite false */
        { mpfr_t a, b; init_neg_inf(a, 53); init_from_double(b, -1e100, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* -Inf < finite true */
        { mpfr_t a, b; init_from_double(a, -1e100, 53); init_neg_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* finite > -Inf false */
        { mpfr_t a, b; init_neg_inf(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* -Inf < +0 true */
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* true diff prec */

        /* +NaN extremes -- +NaN is the maximum; to(+NaN,+NaN)=true (10). */
        { mpfr_t a, b; init_nan(a, 53); init_nan(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* +NaN<=+NaN true */
        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, 1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* +NaN > 1 false */
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_nan(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* 1 < +NaN true */
        { mpfr_t a, b; init_nan(a, 53); init_pos_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* +NaN > +Inf false */
        { mpfr_t a, b; init_pos_inf(a, 53); init_nan(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* +Inf < +NaN true */
        { mpfr_t a, b; init_nan(a, 53); init_neg_inf(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* +NaN > -Inf false */
        { mpfr_t a, b; init_neg_inf(a, 53); init_nan(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* -Inf < +NaN true */
        { mpfr_t a, b; init_nan(a, 53); init_pos_zero(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* +NaN > +0 false */
        { mpfr_t a, b; init_neg_zero(a, 53); init_nan(b, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* -0 < +NaN true */
        { mpfr_t a, b; init_nan(a, 53); init_nan(b, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); } /* +NaN<=+NaN diff prec true */

        /* Same value, different prec -> both directions true (6). */
        { mpfr_t a, b; init_from_double(a, 1.0, 2); init_from_double(b, 1.0, 53); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_from_double(b, 1.0, 2); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -2.0, 1); init_from_double(b, -2.0, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 3.14, 53); init_from_double(b, 3.14, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 100.0, 53); init_from_double(b, 100.0, 256); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 0.5, 1); init_from_double(b, 0.5, 200); emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    /* ============================================================== */
    /* adversarial: 16 -- the signed-zero gate + NaN-reflexivity gate. */
    /*                                                                 */
    /* A naive port that delegates straight to mpfr_lessequal_p (which */
    /* collapses +0/-0 to equal and returns false on NaN) gets the     */
    /* signed-zero cases and ALL NaN cases wrong -- these rows pin     */
    /* exactly that mutation.                                          */
    /* ============================================================== */
    {
        /* Signed-zero: lessequal-only port returns true for BOTH
         * directions (cmp==0); correct gives true/false asymmetric. */
        { mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 53); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* false */
        { mpfr_t a, b; init_neg_zero(a, 53); init_pos_zero(b, 53); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* true */
        { mpfr_t a, b; init_pos_zero(a, 24); init_neg_zero(b, 53); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* false */
        { mpfr_t a, b; init_neg_zero(a, 24); init_pos_zero(b, 53); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* true */
        { mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 24); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* false */
        { mpfr_t a, b; init_neg_zero(a, 53); init_neg_zero(b, 24); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* true (same sign reflexive) */

        /* NaN reflexivity & extremes: lessequal-only port returns false
         * for ALL of these; correct gives true/false per total order. */
        { mpfr_t a, b; init_nan(a, 53); init_nan(b, 53); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* true */
        { mpfr_t a, b; init_nan(a, 64); init_nan(b, 128); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* true */
        { mpfr_t a, b; init_from_double(a, -1.0, 53); init_nan(b, 53); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* true */
        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, -1.0, 53); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* false */
        { mpfr_t a, b; init_nan(a, 53); init_neg_zero(b, 53); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* false */
        { mpfr_t a, b; init_pos_zero(a, 53); init_nan(b, 53); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* true */

        /* Strict-ordered finite different-prec -- normal lessequal path. */
        { mpfr_t a, b; init_from_double(a, 1.0, 2); init_from_double(b, 2.0, 53); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* true */
        { mpfr_t a, b; init_from_double(a, 2.0, 53); init_from_double(b, 1.0, 2); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* false */
        { mpfr_t a, b; init_from_double(a, -1.0, 200); init_from_double(b, 1.0, 53); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* true */
        { mpfr_t a, b; init_from_double(a, 1e100, 53); init_from_double(b, 1e-100, 53); emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); } /* false */
    }

    /* ============================================================== */
    /* fuzz: 60 -- PRNG finite pairs (mix of equal and strict).        */
    /* Unique 64-bit hex seed (digits 0-9 A-F only).                   */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x70741D03DEABCDEFULL);
        const uint64_t precs[5] = { 53, 64, 100, 128, 200 };

        int emitted = 0;
        int case_idx = 0;
        while (emitted < 60) {
            const int equal_pair = (case_idx % 4) == 0;
            case_idx++;

            const uint64_t bits_a = xs64_next(&rng);
            const uint64_t bits_b = equal_pair ? bits_a : xs64_next(&rng);

            const uint64_t exp_a = (bits_a >> 52) & 0x7FF;
            const uint64_t exp_b = (bits_b >> 52) & 0x7FF;
            if (exp_a == 0x7FF || exp_b == 0x7FF) continue; /* skip inf/nan doubles */

            double da, db;
            memcpy(&da, &bits_a, sizeof da);
            memcpy(&db, &bits_b, sizeof db);

            const uint64_t pa = precs[xs64_below(&rng, 5)];
            const uint64_t pb = precs[xs64_below(&rng, 5)];

            mpfr_t a, b;
            init_from_double(a, da, pa);
            init_from_double(b, db, pb);
            emit_case(out, "fuzz", a, b);
            mpfr_clear(a); mpfr_clear(b);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 -- transcribed from mpfr/tests/ttotal_order semantics  */
    /* (IEEE 754-2008 5.10 canonical orderings).                       */
    /* ============================================================== */
    {
        /* -0 < +0 -- the canonical signed-zero total-order distinction. */
        { mpfr_t a, b; init_neg_zero(a, 53); init_pos_zero(b, 53); emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* -Inf < -finite. */
        { mpfr_t a, b; init_neg_inf(a, 53); init_from_double(b, -1.0, 53); emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* +finite < +Inf. */
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_pos_inf(b, 53); emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* +Inf < +NaN. */
        { mpfr_t a, b; init_pos_inf(a, 53); init_nan(b, 53); emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
        /* +NaN <= +NaN reflexive (totalOrder is a total order). */
        { mpfr_t a, b; init_nan(a, 53); init_nan(b, 53); emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    return 0;
}
