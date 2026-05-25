/*
 * golden_driver.c -- Golden master for MPFR's mpfr_eq.
 *
 * C: int mpfr_eq(mpfr_srcptr u, mpfr_srcptr v, unsigned long int n_bits);
 *   Returns non-zero iff the first n_bits bits of u and v agree.
 * Ref: mpfr/src/eq.c L27-L140.
 *
 * Wire: {"inputs":{"u":<mpfr>,"v":<mpfr>,"n_bits":"<dec>"},"output":true|false}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_eq golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr u, mpfr_srcptr v, uint64_t n_bits) {
    const uint64_t t0 = now_ns();
    const int r = mpfr_eq(u, v, (unsigned long)n_bits);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "u", u);
    jl_kv_mpfr(out, 0, "v", v);
    jl_kv_u64(out, 0, "n_bits", n_bits);
    jl_end_inputs(out);
    jl_output_scalar_bool(out, r);
    jl_finish(out, elapsed);
}

static inline void mk_from_double(mpfr_ptr x, double d, mpfr_prec_t prec) {
    mpfr_init2(x, prec); mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void mk_inf(mpfr_ptr x, int sign, mpfr_prec_t prec) {
    mpfr_init2(x, prec); mpfr_set_inf(x, sign);
}
static inline void mk_zero(mpfr_ptr x, int sign, mpfr_prec_t prec) {
    mpfr_init2(x, prec); mpfr_set_zero(x, sign);
}
static inline void mk_nan(mpfr_ptr x, mpfr_prec_t prec) {
    mpfr_init2(x, prec); mpfr_set_nan(x);
}

int main(void) {
    FILE *out = stdout;
    /* happy: 20 -- pairs of normals, common precs and n_bits. */
    {
        mpfr_t u, v;
        mk_from_double(u, 3.14, 53); mk_from_double(v, 3.14, 53);
        emit_case(out, "happy", u, v, 53);
        emit_case(out, "happy", u, v, 1);
        emit_case(out, "happy", u, v, 10);
        emit_case(out, "happy", u, v, 100);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 3.14, 53); mk_from_double(v, 3.15, 53);
        emit_case(out, "happy", u, v, 53);
        emit_case(out, "happy", u, v, 5);
        emit_case(out, "happy", u, v, 1);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 1.0, 53); mk_from_double(v, 2.0, 53);
        emit_case(out, "happy", u, v, 1);
        emit_case(out, "happy", u, v, 53);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 1.0, 53); mk_from_double(v, -1.0, 53);
        emit_case(out, "happy", u, v, 1);
        emit_case(out, "happy", u, v, 53);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 100.5, 53); mk_from_double(v, 100.5, 53);
        emit_case(out, "happy", u, v, 53);
        emit_case(out, "happy", u, v, 200);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 0.5, 53); mk_from_double(v, 0.5, 53);
        emit_case(out, "happy", u, v, 53);
        emit_case(out, "happy", u, v, 1);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 1.5, 24); mk_from_double(v, 1.5, 53);  /* same value, different precs */
        emit_case(out, "happy", u, v, 24);
        emit_case(out, "happy", u, v, 53);
        emit_case(out, "happy", u, v, 100);
        mpfr_clear(u); mpfr_clear(v);
    }
    /* edge: 30 -- NaN, Inf, Zero special cases. */
    { mpfr_t u, v; mk_nan(u, 53); mk_nan(v, 53); emit_case(out, "edge", u, v, 10); mpfr_clear(u); mpfr_clear(v); }
    { mpfr_t u, v; mk_nan(u, 53); mk_from_double(v, 1.0, 53); emit_case(out, "edge", u, v, 10); mpfr_clear(u); mpfr_clear(v); }
    { mpfr_t u, v; mk_from_double(u, 1.0, 53); mk_nan(v, 53); emit_case(out, "edge", u, v, 10); mpfr_clear(u); mpfr_clear(v); }
    { mpfr_t u, v; mk_inf(u, +1, 53); mk_inf(v, +1, 53); emit_case(out, "edge", u, v, 10); mpfr_clear(u); mpfr_clear(v); }
    { mpfr_t u, v; mk_inf(u, +1, 53); mk_inf(v, -1, 53); emit_case(out, "edge", u, v, 10); mpfr_clear(u); mpfr_clear(v); }
    { mpfr_t u, v; mk_inf(u, -1, 53); mk_inf(v, -1, 53); emit_case(out, "edge", u, v, 10); mpfr_clear(u); mpfr_clear(v); }
    { mpfr_t u, v; mk_inf(u, +1, 53); mk_from_double(v, 1.0, 53); emit_case(out, "edge", u, v, 10); mpfr_clear(u); mpfr_clear(v); }
    { mpfr_t u, v; mk_zero(u, +1, 53); mk_zero(v, +1, 53); emit_case(out, "edge", u, v, 10); mpfr_clear(u); mpfr_clear(v); }
    { mpfr_t u, v; mk_zero(u, +1, 53); mk_zero(v, -1, 53); emit_case(out, "edge", u, v, 10); mpfr_clear(u); mpfr_clear(v); }
    { mpfr_t u, v; mk_zero(u, -1, 53); mk_zero(v, -1, 53); emit_case(out, "edge", u, v, 10); mpfr_clear(u); mpfr_clear(v); }
    { mpfr_t u, v; mk_zero(u, +1, 53); mk_from_double(v, 0.001, 53); emit_case(out, "edge", u, v, 10); mpfr_clear(u); mpfr_clear(v); }
    /* exponent differences */
    {
        mpfr_t u, v;
        mk_from_double(u, 1.0, 53); mk_from_double(v, 2.0, 53);
        emit_case(out, "edge", u, v, 1);
        emit_case(out, "edge", u, v, 10);
        emit_case(out, "edge", u, v, 53);
        mpfr_clear(u); mpfr_clear(v);
    }
    /* n_bits=0 with various pairs */
    {
        mpfr_t u, v;
        mk_from_double(u, 1.0, 53); mk_from_double(v, 1.0, 53);
        emit_case(out, "edge", u, v, 0);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 1.0, 53); mk_from_double(v, 2.0, 53);
        emit_case(out, "edge", u, v, 0);
        mpfr_clear(u); mpfr_clear(v);
    }
    /* limb boundary precs */
    {
        mpfr_t u, v;
        mk_from_double(u, 3.14, 64); mk_from_double(v, 3.14, 64);
        emit_case(out, "edge", u, v, 64);
        emit_case(out, "edge", u, v, 65);
        emit_case(out, "edge", u, v, 128);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 1.5, 1); mk_from_double(v, 1.5, 1);
        emit_case(out, "edge", u, v, 1);
        emit_case(out, "edge", u, v, 2);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 1.5, 128); mk_from_double(v, 1.5, 128);
        emit_case(out, "edge", u, v, 64);
        emit_case(out, "edge", u, v, 128);
        emit_case(out, "edge", u, v, 256);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 0.1, 200); mk_from_double(v, 0.1, 200);
        emit_case(out, "edge", u, v, 200);
        emit_case(out, "edge", u, v, 100);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mpfr_init2(u, 53); mpfr_init2(v, 53);
        mpfr_set_str(u, "1.0001", 2, MPFR_RNDN);
        mpfr_set_str(v, "1.0010", 2, MPFR_RNDN);
        emit_case(out, "edge", u, v, 1);  /* agree on 1 bit (the leading 1) */
        emit_case(out, "edge", u, v, 3);
        emit_case(out, "edge", u, v, 5);
        mpfr_clear(u); mpfr_clear(v);
    }
    /* adversarial: 12 -- very large n_bits, NaN-vs-Inf mixes, exact-boundary cases */
    {
        mpfr_t u, v;
        mk_from_double(u, 1.0, 53); mk_from_double(v, 1.0, 53);
        emit_case(out, "adversarial", u, v, 1000);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_nan(u, 53); mk_inf(v, +1, 53);
        emit_case(out, "adversarial", u, v, 10);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_inf(u, +1, 53); mk_zero(v, +1, 53);
        emit_case(out, "adversarial", u, v, 1);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 1.0, 53); mk_from_double(v, 1.0000000001, 53);
        emit_case(out, "adversarial", u, v, 30);
        emit_case(out, "adversarial", u, v, 53);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 1.0, 200); mk_from_double(v, 1.0, 200);
        emit_case(out, "adversarial", u, v, 200);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 0.5, 53); mk_from_double(v, 0.25, 53);
        emit_case(out, "adversarial", u, v, 1);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 3.14, 24); mk_from_double(v, 3.14, 53);
        emit_case(out, "adversarial", u, v, 24);
        emit_case(out, "adversarial", u, v, 53);
        mpfr_clear(u); mpfr_clear(v);
    }
    /* fuzz: 50 -- random pairs at random precisions and n_bits. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x1357902468ACEFFFULL);
        for (int rep = 0; rep < 50; ++rep) {
            const mpfr_prec_t p = (mpfr_prec_t)(2 + xs64_below(&rng, 200));
            const uint64_t hi1 = xs64_next(&rng);
            const uint64_t hi2 = xs64_next(&rng);
            const double d1 = (double)(hi1 & 0xFFFFFFFFULL) / 1e7;
            const double d2 = (double)(hi2 & 0xFFFFFFFFULL) / 1e7;
            const uint64_t nb = xs64_below(&rng, (uint64_t)p + 10);
            mpfr_t u, v;
            mpfr_init2(u, p); mpfr_init2(v, p);
            mpfr_set_d(u, d1, MPFR_RNDN);
            mpfr_set_d(v, d2, MPFR_RNDN);
            emit_case(out, "fuzz", u, v, nb);
            mpfr_clear(u); mpfr_clear(v);
        }
    }
    /* mined: 5 -- from mpfr/tests/teq.c. */
    {
        mpfr_t u, v;
        mk_from_double(u, 1.0, 53); mk_from_double(v, 1.0, 53);
        emit_case(out, "mined", u, v, 53);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 0.0, 53); mk_from_double(v, 1.0, 53);
        emit_case(out, "mined", u, v, 1);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_zero(u, +1, 53); mk_zero(v, +1, 53);
        emit_case(out, "mined", u, v, 0);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_nan(u, 53); mk_nan(v, 53);
        emit_case(out, "mined", u, v, 1);
        mpfr_clear(u); mpfr_clear(v);
    }
    {
        mpfr_t u, v;
        mk_from_double(u, 2.0, 53); mk_from_double(v, 4.0, 53);
        emit_case(out, "mined", u, v, 1);
        mpfr_clear(u); mpfr_clear(v);
    }
    return 0;
}
