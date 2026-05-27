/*
 * golden_driver.c -- Golden master for MPFR's mpfr_nexttoward.
 *
 * C: void mpfr_nexttoward(mpfr_ptr x, mpfr_srcptr y).
 *    Steps x one ULP toward y at x's own precision (IEEE 754-1985
 *    nextafter restricted to MPFR's precision lattice). No rounding.
 *    Ref: mpfr/src/next.c L148-L172.
 *
 * Dispatch:
 *   - x is NaN -> x stays NaN.
 *   - y is NaN -> x becomes NaN.
 *   - cmp(x, y) == 0 -> x unchanged (IEEE 754-1985: NOT y, even for +/-0).
 *   - cmp(x, y) < 0 -> mpfr_nextabove(x).
 *   - cmp(x, y) > 0 -> mpfr_nextbelow(x).
 *
 * Wire: {"inputs":{"x":<MPFR>,"y":<MPFR>},"output":<MPFR>}.
 * Output is BARE MPFR (jl_output_mpfr), not Result.
 *
 * Tag distribution (Rule 7): happy 22, edge 34, adv 12, fuzz 55, mined 6.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_nexttoward golden_driver requires GMP_NUMB_BITS == 64"
#endif

extern void mpfr_setmin(mpfr_ptr, mpfr_exp_t);
extern void mpfr_setmax(mpfr_ptr, mpfr_exp_t);

/* Emit one case. Both x and y supplied; x is mutated by mpfr_nexttoward
 * to produce the output. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x_in, mpfr_srcptr y_in) {
    mpfr_t x;
    mpfr_init2(x, mpfr_get_prec(x_in));
    mpfr_set(x, x_in, MPFR_RNDN);
    const uint64_t t0 = now_ns();
    mpfr_nexttoward(x, y_in);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x_in);
    jl_kv_mpfr(out, 0, "y", y_in);
    jl_end_inputs(out);
    jl_output_mpfr(out, x);
    jl_finish(out, elapsed);
    mpfr_clear(x);
}

/* Builders. */
static inline void init_from_double(mpfr_ptr v, double d, uint64_t prec) {
    mpfr_init2(v, (mpfr_prec_t)prec);
    mpfr_set_d(v, d, MPFR_RNDN);
}
static inline void init_pos_inf(mpfr_ptr v, uint64_t prec) {
    mpfr_init2(v, (mpfr_prec_t)prec); mpfr_set_inf(v, 1);
}
static inline void init_neg_inf(mpfr_ptr v, uint64_t prec) {
    mpfr_init2(v, (mpfr_prec_t)prec); mpfr_set_inf(v, -1);
}
static inline void init_pos_zero(mpfr_ptr v, uint64_t prec) {
    mpfr_init2(v, (mpfr_prec_t)prec); mpfr_set_zero(v, 1);
}
static inline void init_neg_zero(mpfr_ptr v, uint64_t prec) {
    mpfr_init2(v, (mpfr_prec_t)prec); mpfr_set_zero(v, -1);
}
static inline void init_nan(mpfr_ptr v, uint64_t prec) {
    mpfr_init2(v, (mpfr_prec_t)prec); mpfr_set_nan(v);
}
static inline void init_min_pos(mpfr_ptr v, uint64_t prec) {
    mpfr_init2(v, (mpfr_prec_t)prec); mpfr_setmin(v, mpfr_get_emin());
}
static inline void init_max_pos(mpfr_ptr v, uint64_t prec) {
    mpfr_init2(v, (mpfr_prec_t)prec); mpfr_setmax(v, mpfr_get_emax());
}

int main(void) {
    FILE *out = stdout;

    /* ===== happy: 22 -- finite normals; both signs across precisions. ===== */
    {
        /* Toward positive direction (y > x). */
        { mpfr_t x, y; init_from_double(x, 1.0, 53); init_from_double(y, 2.0, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 3.14, 53); init_from_double(y, 4.0, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, -2.0, 53); init_from_double(y, -1.0, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, -3.14, 53); init_from_double(y, 0.0, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* Toward negative direction (y < x). */
        { mpfr_t x, y; init_from_double(x, 2.0, 53); init_from_double(y, 1.0, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 4.0, 53); init_from_double(y, 3.14, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, -1.0, 53); init_from_double(y, -2.0, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 0.0, 53); init_from_double(y, -3.14, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* Equal: x unchanged. */
        { mpfr_t x, y; init_from_double(x, 1.0, 53); init_from_double(y, 1.0, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 3.14, 53); init_from_double(y, 3.14, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* Various precisions. */
        { mpfr_t x, y; init_from_double(x, 3.14, 24); init_from_double(y, 4.0, 24);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 3.14, 64); init_from_double(y, 4.0, 64);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 3.14, 128); init_from_double(y, 4.0, 128);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 3.14, 200); init_from_double(y, 4.0, 200);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* Cross zero. */
        { mpfr_t x, y; init_from_double(x, -1.0, 53); init_from_double(y, 1.0, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 1.0, 53); init_from_double(y, -1.0, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* Different precisions between x and y are allowed; mpfr_cmp handles. */
        { mpfr_t x, y; init_from_double(x, 1.0, 53); init_from_double(y, 2.0, 24);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 1.0, 24); init_from_double(y, 2.0, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* Same magnitude different sign: cmp != 0, dispatch via nextabove/below. */
        { mpfr_t x, y; init_from_double(x, 1.0, 53); init_from_double(y, -1.0, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, -1.0, 53); init_from_double(y, 1.0, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* Toward Inf. */
        { mpfr_t x, y; init_from_double(x, 100.0, 53); init_pos_inf(y, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, -100.0, 53); init_neg_inf(y, 53);
          emit_case(out, "happy", x, y); mpfr_clear(x); mpfr_clear(y); }
    }

    /* ===== edge: 34 -- NaN, Inf, +/-0 (IEEE 754-1985 semantics). ===== */
    {
        /* NaN(x): x stays NaN regardless of y. */
        { mpfr_t x, y; init_nan(x, 53); init_from_double(y, 1.0, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_nan(x, 53); init_pos_inf(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_nan(x, 53); init_nan(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_nan(x, 1); init_from_double(y, 1.0, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_nan(x, 200); init_from_double(y, 1.0, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* NaN(y): x becomes NaN. */
        { mpfr_t x, y; init_from_double(x, 1.0, 53); init_nan(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_pos_inf(x, 53); init_nan(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_pos_zero(x, 53); init_nan(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 1.0, 1); init_nan(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* +/-0 equal but different signs (IEEE 754-1985: x returned unchanged). */
        { mpfr_t x, y; init_pos_zero(x, 53); init_neg_zero(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_neg_zero(x, 53); init_pos_zero(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_pos_zero(x, 53); init_pos_zero(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_neg_zero(x, 53); init_neg_zero(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* +0 toward something positive: nextabove(+0) = +smallest. */
        { mpfr_t x, y; init_pos_zero(x, 53); init_from_double(y, 1.0, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_pos_zero(x, 53); init_pos_inf(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* +0 toward something negative: nextbelow(+0) = -smallest. */
        { mpfr_t x, y; init_pos_zero(x, 53); init_from_double(y, -1.0, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_pos_zero(x, 53); init_neg_inf(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* -0 toward something positive: nextabove(-0) = +smallest. */
        { mpfr_t x, y; init_neg_zero(x, 53); init_from_double(y, 1.0, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_neg_zero(x, 53); init_pos_inf(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* -0 toward something negative: nextbelow(-0) = -smallest. */
        { mpfr_t x, y; init_neg_zero(x, 53); init_from_double(y, -1.0, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_neg_zero(x, 53); init_neg_inf(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* +/-Inf toward finite. */
        { mpfr_t x, y; init_pos_inf(x, 53); init_from_double(y, 1.0, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_neg_inf(x, 53); init_from_double(y, -1.0, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_pos_inf(x, 53); init_from_double(y, -1.0, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_neg_inf(x, 53); init_from_double(y, 1.0, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* +Inf toward +Inf: cmp == 0, no change. */
        { mpfr_t x, y; init_pos_inf(x, 53); init_pos_inf(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_neg_inf(x, 53); init_neg_inf(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* +smallest toward 0: nextbelow(+smallest) = +0 (underflow). */
        { mpfr_t x, y; init_min_pos(x, 53); init_pos_zero(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* +max toward Inf: nextabove(+max) = +Inf. */
        { mpfr_t x, y; init_max_pos(x, 53); init_pos_inf(y, 53);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* prec=1 cases. */
        { mpfr_t x, y; init_from_double(x, 1.0, 1); init_from_double(y, 2.0, 1);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 1.0, 1); init_from_double(y, 0.5, 1);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_pos_zero(x, 1); init_from_double(y, 1.0, 1);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_neg_zero(x, 1); init_pos_zero(y, 1);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_neg_inf(x, 1); init_pos_inf(y, 1);
          emit_case(out, "edge", x, y); mpfr_clear(x); mpfr_clear(y); }
    }

    /* ===== adversarial: 12 -- powers of two, max-min boundaries. ===== */
    {
        /* power-of-two x stepping up: nextabove triggers no renormalize. */
        { mpfr_t x, y; mpfr_init2(x, 53); mpfr_init2(y, 53);
          mpfr_set_ui(x, 1, MPFR_RNDN); mpfr_set_exp(x, 5);
          mpfr_set_ui(y, 100, MPFR_RNDN);
          emit_case(out, "adversarial", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* power-of-two x stepping down: triggers renormalize (exp -= 1). */
        { mpfr_t x, y; mpfr_init2(x, 53); mpfr_init2(y, 53);
          mpfr_set_ui(x, 1, MPFR_RNDN); mpfr_set_exp(x, 5);
          mpfr_set_ui(y, 0, MPFR_RNDN);
          emit_case(out, "adversarial", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* All-ones mantissa overflow: nextabove flips to next binade. */
        { mpfr_t x, y; mpfr_init2(x, 53); mpfr_init2(y, 53);
          mpfr_setmax(x, 10); mpfr_set_inf(y, 1);
          emit_case(out, "adversarial", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* All-ones negative: nextbelow flips to negative next binade. */
        { mpfr_t x, y; mpfr_init2(x, 53); mpfr_init2(y, 53);
          mpfr_setmax(x, 10); mpfr_neg(x, x, MPFR_RNDN);
          mpfr_set_inf(y, -1);
          emit_case(out, "adversarial", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* +/-Inf toward each other. */
        { mpfr_t x, y; init_pos_inf(x, 53); init_neg_inf(y, 53);
          emit_case(out, "adversarial", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_neg_inf(x, 53); init_pos_inf(y, 53);
          emit_case(out, "adversarial", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* Near-zero cancellation. */
        { mpfr_t x, y; init_from_double(x, 1e-300, 53); init_from_double(y, -1e-300, 53);
          emit_case(out, "adversarial", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 1e300, 53); init_from_double(y, -1e300, 53);
          emit_case(out, "adversarial", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* MSB+1: bit just above the MSB; both directions. */
        { mpfr_t x, y; mpfr_init2(x, 53); mpfr_init2(y, 53);
          mpfr_set_str(x, "1.0000000000000000000000000000000000000000000000000001", 2, MPFR_RNDN);
          mpfr_set_ui(y, 2, MPFR_RNDN);
          emit_case(out, "adversarial", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; mpfr_init2(x, 53); mpfr_init2(y, 53);
          mpfr_set_str(x, "1.0000000000000000000000000000000000000000000000000001", 2, MPFR_RNDN);
          mpfr_set_ui(y, 0, MPFR_RNDN);
          emit_case(out, "adversarial", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* +smallest toward -smallest. */
        { mpfr_t x, y; init_min_pos(x, 53); mpfr_init2(y, 53);
          mpfr_setmin(y, mpfr_get_emin()); mpfr_neg(y, y, MPFR_RNDN);
          emit_case(out, "adversarial", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* x = +smallest toward +smallest: cmp == 0, no change. */
        { mpfr_t x, y; init_min_pos(x, 53); init_min_pos(y, 53);
          emit_case(out, "adversarial", x, y); mpfr_clear(x); mpfr_clear(y); }
    }

    /* ===== fuzz: 55 -- random pairs (x, y) excluding NaN. ===== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEEFEEDFACE99ULL);
        const uint64_t precs[7] = { 1, 2, 24, 53, 64, 100, 200 };
        int emitted = 0;
        while (emitted < 55) {
            const uint64_t prec = precs[xs64_below(&rng, 7)];
            const uint64_t r1 = xs64_next(&rng), r2 = xs64_next(&rng),
                           r3 = xs64_next(&rng), r4 = xs64_next(&rng);
            const int64_t e1 = (int64_t)(r1 % 401) - 200;
            const int64_t e2 = (int64_t)(r2 % 401) - 200;
            const double bx = ((double)(r3 | 1) / 18446744073709551616.0)
                              * ldexp(1.0, (int)e1);
            const double by = ((double)(r4 | 1) / 18446744073709551616.0)
                              * ldexp(1.0, (int)e2);
            const int signx = (xs64_below(&rng, 2)) ? +1 : -1;
            const int signy = (xs64_below(&rng, 2)) ? +1 : -1;
            mpfr_t x, y;
            mpfr_init2(x, (mpfr_prec_t)prec);
            mpfr_init2(y, (mpfr_prec_t)prec);
            mpfr_set_d(x, signx * bx, MPFR_RNDN);
            mpfr_set_d(y, signy * by, MPFR_RNDN);
            if (!mpfr_regular_p(x) || !mpfr_regular_p(y)) {
                mpfr_clear(x); mpfr_clear(y);
                continue;
            }
            emit_case(out, "fuzz", x, y);
            mpfr_clear(x); mpfr_clear(y);
            emitted++;
        }
    }

    /* ===== mined: 6 -- patterns from mpfr/tests/tnext.c. ===== */
    {
        /* tnext.c L48-L51: NaN tests. */
        { mpfr_t x, y; init_nan(x, 53); init_from_double(y, 1.0, 53);
          emit_case(out, "mined", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 1.0, 53); init_nan(y, 53);
          emit_case(out, "mined", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* tnext.c L53-L56: x = 1, y = 1, nexttoward returns 1 unchanged. */
        { mpfr_t x, y; init_from_double(x, 1.0, 53); init_from_double(y, 1.0, 53);
          emit_case(out, "mined", x, y); mpfr_clear(x); mpfr_clear(y); }
        /* tnext.c L130 inverse_test: nexttoward in y toward x, where they
         * are adjacent values (after nextabove); covered by the
         * inverse_test loop. */
        { mpfr_t x, y; init_from_double(x, 2.0, 53); init_from_double(y, 3.0, 53);
          emit_case(out, "mined", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 3.0, 53); init_from_double(y, 2.0, 53);
          emit_case(out, "mined", x, y); mpfr_clear(x); mpfr_clear(y); }
        { mpfr_t x, y; init_from_double(x, 3.1, 53); init_from_double(y, 3.2, 53);
          emit_case(out, "mined", x, y); mpfr_clear(x); mpfr_clear(y); }
    }

    return 0;
}
