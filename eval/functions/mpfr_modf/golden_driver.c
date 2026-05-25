/*
 * golden_driver.c -- Golden master for MPFR's mpfr_modf.
 *
 * C signature: int mpfr_modf(mpfr_ptr iop, mpfr_ptr fop, mpfr_srcptr op, mpfr_rnd_t rnd)
 * Ref: mpfr/src/modf.c L24-L98.
 *
 * The C function packs two ternary values into the int return via the
 * INEX macro (mpfr-impl.h L1200-L1201):
 *   INEX(y,z) = INEXPOS(y) | (INEXPOS(z) << 2)
 *   INEXPOS(v) = (v != 0) + (v < 0)
 *     == 0 if v==0, 1 if v>0, 2 if v<0
 *
 * We unpack INEX back to separate inexi and inexf for the TS port's
 * {iop: Result, fop: Result} return shape.
 *
 * Wire: {"inputs":{"x":<mpfr>,"iprec":"<dec>","fprec":"<dec>","rnd":"RND_"},
 *        "output":{"iop":{"value":<mpfr>,"ternary":<int>},
 *                  "fop":{"value":<mpfr>,"ternary":<int>}}}.
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_modf golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))

/* Inverse of INEXPOS: 0->0, 1->+1, 2->-1. */
static inline int inexpos_to_ternary(int p) {
    return (p == 0) ? 0 : (p == 1) ? 1 : -1;
}

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x,
                             uint64_t iprec, uint64_t fprec,
                             mpfr_rnd_t rnd) {
    assert(iprec >= 1 && iprec <= TS_PREC_MAX);
    assert(fprec >= 1 && fprec <= TS_PREC_MAX);
    mpfr_t iop, fop;
    mpfr_init2(iop, (mpfr_prec_t)iprec);
    mpfr_init2(fop, (mpfr_prec_t)fprec);

    const uint64_t t0 = now_ns();
    const int packed = mpfr_modf(iop, fop, x, rnd);
    const uint64_t elapsed = now_ns() - t0;

    /* Unpack: low 2 bits -> inexi, next 2 bits -> inexf. */
    const int inexi = inexpos_to_ternary(packed & 0x3);
    const int inexf = inexpos_to_ternary((packed >> 2) & 0x3);

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_u64(out, 0, "iprec", iprec);
    jl_kv_u64(out, 0, "fprec", fprec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);

    /* Emit the nested-pair output object. */
    jl_output_begin_object(out);
    /* iop: {value, ternary} */
    fputs("\"iop\":{", out);
    jl_kv_mpfr(out, 1, "value", iop);
    jl_kv_int(out, 0, "ternary", inexi);
    fputs("}", out);
    /* fop: {value, ternary} */
    fputs(",\"fop\":{", out);
    jl_kv_mpfr(out, 1, "value", fop);
    jl_kv_int(out, 0, "ternary", inexf);
    fputs("}", out);
    jl_output_end_object(out);

    jl_finish(out, elapsed);

    mpfr_clear(iop);
    mpfr_clear(fop);
}

static inline void emit_from_double(FILE *out, const char *tag,
                                    double d, uint64_t xprec,
                                    uint64_t iprec, uint64_t fprec,
                                    mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec); mpfr_set_d(x, d, MPFR_RNDN);
    emit_case(out, tag, x, iprec, fprec, rnd);
    mpfr_clear(x);
}
static inline void emit_nan(FILE *out, const char *tag,
                            uint64_t xprec, uint64_t iprec, uint64_t fprec,
                            mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec); mpfr_set_nan(x);
    emit_case(out, tag, x, iprec, fprec, rnd);
    mpfr_clear(x);
}
static inline void emit_inf(FILE *out, const char *tag, int sign,
                            uint64_t xprec, uint64_t iprec, uint64_t fprec,
                            mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec); mpfr_set_inf(x, sign);
    emit_case(out, tag, x, iprec, fprec, rnd);
    mpfr_clear(x);
}
static inline void emit_zero(FILE *out, const char *tag, int sign,
                             uint64_t xprec, uint64_t iprec, uint64_t fprec,
                             mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec); mpfr_set_zero(x, sign);
    emit_case(out, tag, x, iprec, fprec, rnd);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = { MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA };

    /* happy: 20 -- ordinary decimals split into int + frac parts. */
    emit_from_double(out, "happy", 2.5, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", -2.5, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", 3.14, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", -3.14, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", 10.5, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", -10.5, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", 0.5, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", -0.5, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", 1.0, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", -1.0, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", 100.25, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", -100.25, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", 1.5, 53, 53, 53, MPFR_RNDZ);
    emit_from_double(out, "happy", 1.5, 53, 53, 53, MPFR_RNDU);
    emit_from_double(out, "happy", 1.5, 53, 53, 53, MPFR_RNDD);
    emit_from_double(out, "happy", 1.5, 53, 53, 53, MPFR_RNDA);
    emit_from_double(out, "happy", 7.875, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", -7.875, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", 0.1, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "happy", 0.9, 53, 53, 53, MPFR_RNDN);

    /* edge: 30 -- specials, |op|<1, no-frac, asymmetric precs. */
    emit_nan(out, "edge", 53, 53, 53, MPFR_RNDN);
    emit_nan(out, "edge", 53, 1, 1, MPFR_RNDN);
    emit_inf(out, "edge", +1, 53, 53, 53, MPFR_RNDN);
    emit_inf(out, "edge", -1, 53, 53, 53, MPFR_RNDN);
    emit_inf(out, "edge", +1, 53, 1, 1, MPFR_RNDN);
    emit_inf(out, "edge", -1, 53, 1, 1, MPFR_RNDN);
    emit_zero(out, "edge", +1, 53, 53, 53, MPFR_RNDN);
    emit_zero(out, "edge", -1, 53, 53, 53, MPFR_RNDN);
    emit_zero(out, "edge", +1, 53, 1, 1, MPFR_RNDN);
    emit_zero(out, "edge", -1, 53, 1, 1, MPFR_RNDN);
    /* |op| < 1 -- iop is +/-0. */
    emit_from_double(out, "edge", 0.25, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "edge", -0.25, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "edge", 0.999, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "edge", -0.999, 53, 53, 53, MPFR_RNDN);
    /* Exact integers (op has no frac part) -- fop is +/-0. */
    emit_from_double(out, "edge", 1.0, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "edge", -1.0, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "edge", 42.0, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "edge", -42.0, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "edge", 1024.0, 53, 53, 53, MPFR_RNDN);
    /* Asymmetric precs. */
    emit_from_double(out, "edge", 3.5, 53, 5, 5, MPFR_RNDN);
    emit_from_double(out, "edge", 3.5, 53, 53, 5, MPFR_RNDN);
    emit_from_double(out, "edge", 3.5, 53, 5, 53, MPFR_RNDN);
    emit_from_double(out, "edge", 100.5, 53, 100, 10, MPFR_RNDN);
    emit_from_double(out, "edge", 100.5, 53, 10, 100, MPFR_RNDN);
    /* prec=1 stress. */
    emit_from_double(out, "edge", 1.5, 53, 1, 1, MPFR_RNDN);
    emit_from_double(out, "edge", -1.5, 53, 1, 1, MPFR_RNDN);
    emit_from_double(out, "edge", 1.5, 53, 1, 1, MPFR_RNDZ);
    emit_from_double(out, "edge", 1.5, 53, 1, 1, MPFR_RNDU);
    /* High-prec input. */
    emit_from_double(out, "edge", 3.14, 200, 200, 200, MPFR_RNDN);
    emit_from_double(out, "edge", 3.14, 200, 200, 200, MPFR_RNDZ);

    /* adversarial: 12 -- precision-truncation pressure. */
    emit_from_double(out, "adversarial", 3.14, 53, 2, 2, MPFR_RNDN);
    emit_from_double(out, "adversarial", 3.14, 53, 2, 2, MPFR_RNDZ);
    emit_from_double(out, "adversarial", 3.14, 53, 2, 2, MPFR_RNDU);
    emit_from_double(out, "adversarial", 3.14, 53, 2, 2, MPFR_RNDD);
    emit_from_double(out, "adversarial", 3.14, 53, 2, 2, MPFR_RNDA);
    emit_from_double(out, "adversarial", -3.14, 53, 2, 2, MPFR_RNDU);
    emit_from_double(out, "adversarial", -3.14, 53, 2, 2, MPFR_RNDD);
    /* Near-integer (tiny frac). */
    emit_from_double(out, "adversarial", 1.0000000001, 53, 53, 53, MPFR_RNDN);
    emit_from_double(out, "adversarial", -1.0000000001, 53, 53, 53, MPFR_RNDN);
    /* Near-power-of-2 boundary. */
    emit_from_double(out, "adversarial", 2.999999, 53, 1, 53, MPFR_RNDN);
    /* Tie-breakers in both parts. */
    emit_from_double(out, "adversarial", 5.5, 53, 2, 2, MPFR_RNDN);
    emit_from_double(out, "adversarial", -5.5, 53, 2, 2, MPFR_RNDN);

    /* fuzz: 50 random (x, iprec, fprec, rnd). */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x0DDBEEFCAFE5BABEULL);
        const uint64_t precs[6] = { 1, 2, 24, 53, 64, 100 };
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t xprec = precs[xs64_below(&rng, 6)];
            const uint64_t iprec = precs[xs64_below(&rng, 6)];
            const uint64_t fprec = precs[xs64_below(&rng, 6)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            const uint64_t kc = xs64_below(&rng, 10);
            mpfr_t x;
            mpfr_init2(x, (mpfr_prec_t)xprec);
            switch (kc) {
                case 0: mpfr_set_nan(x); break;
                case 1: mpfr_set_inf(x, +1); break;
                case 2: mpfr_set_inf(x, -1); break;
                case 3: mpfr_set_zero(x, +1); break;
                case 4: mpfr_set_zero(x, -1); break;
                default: {
                    const int neg = (xs64_below(&rng, 2) == 0) ? +1 : -1;
                    const double v = ((double)(xs64_below(&rng, 100000) + 1) / 1000.0) * neg;
                    mpfr_set_d(x, v, MPFR_RNDN);
                    break;
                }
            }
            emit_case(out, "fuzz", x, iprec, fprec, rnd);
            mpfr_clear(x);
        }
    }

    /* mined: 5 -- patterns from mpfr/tests/tmodf.c. */
    /* check_nans (L66-L97): NaN, +Inf, -Inf. */
    emit_nan(out, "mined", 123, 123, 123, MPFR_RNDN);
    emit_inf(out, "mined", +1, 123, 123, 123, MPFR_RNDN);
    emit_inf(out, "mined", -1, 123, 123, 123, MPFR_RNDN);
    /* check (L24-L63): canonical decimal "2.5", split as iop=2, fop=0.5. */
    emit_from_double(out, "mined", 2.5, 24, 24, 24, MPFR_RNDN);
    /* Negative variant. */
    emit_from_double(out, "mined", -2.5, 24, 24, 24, MPFR_RNDN);

    return 0;
}
