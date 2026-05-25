/*
 * golden_driver.c -- Golden master for MPFR's static mpfr_compound_near_one.
 *
 * The C function is `static` in mpfr/src/compound.c, NOT exported as a
 * libmpfr symbol. The driver re-implements the algorithm verbatim
 * (golden-driver-substitute pattern per ADR 0002) using only the
 * exported mpfr_set_ui, mpfr_nextabove, mpfr_nextbelow primitives.
 *
 * The algorithm (mpfr/src/compound.c L31-L54):
 *   mpfr_set_ui(y, 1, rnd);   // exact
 *   if (rnd == RNDN || (s>0 && rnd in {RNDZ, RNDD})
 *                   || (s<0 && rnd in {RNDA, RNDU})):
 *       return -s;
 *   else if (s > 0):  // must be RNDA or RNDU
 *       mpfr_nextabove(y); return +1;
 *   else:             // s < 0, RNDZ or RNDD
 *       mpfr_nextbelow(y); return -1;
 *
 * Wire: {"inputs":{"prec":"<dec>","s":<int>,"rnd":"RND_"},"output":{"value":<mpfr>,"ternary":<int>}}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_compound_near_one golden_driver requires GMP_NUMB_BITS == 64"
#endif

static int compute_compound_near_one(mpfr_ptr y, int s, mpfr_rnd_t rnd) {
    mpfr_set_ui(y, 1, rnd);  /* exact */
    /* The C also checks rnd == MPFR_RNDF, but RNDF is unsupported in TS;
     * skip it. */
    if (rnd == MPFR_RNDN
        || (s > 0 && (rnd == MPFR_RNDZ || rnd == MPFR_RNDD))
        || (s < 0 && (rnd == MPFR_RNDA || rnd == MPFR_RNDU))) {
        return -s;
    } else if (s > 0) {
        mpfr_nextabove(y);
        return +1;
    } else {
        mpfr_nextbelow(y);
        return -1;
    }
}

static inline void emit_case(FILE *out, const char *tag,
                              uint64_t prec, int s, mpfr_rnd_t rnd) {
    assert(prec >= 1);
    assert(s == +1 || s == -1);
    mpfr_t y;
    mpfr_init2(y, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int t = compute_compound_near_one(y, s, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "prec", prec);
    jl_kv_int(out, 0, "s", s);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, y, t);
    jl_finish(out, elapsed);
    mpfr_clear(y);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = { MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA };
    /* happy: 20 -- 2 signs x 5 modes at prec 53 and 64. */
    for (int si = 0; si < 2; ++si) {
        const int s = si == 0 ? +1 : -1;
        for (int ri = 0; ri < 5; ++ri) {
            emit_case(out, "happy", 53, s, RNDS[ri]);
        }
    }
    for (int si = 0; si < 2; ++si) {
        const int s = si == 0 ? +1 : -1;
        for (int ri = 0; ri < 5; ++ri) {
            emit_case(out, "happy", 64, s, RNDS[ri]);
        }
    }
    /* edge: 30 -- prec=1, limb boundaries. */
    for (int si = 0; si < 2; ++si) {
        const int s = si == 0 ? +1 : -1;
        for (int ri = 0; ri < 5; ++ri) emit_case(out, "edge", 1, s, RNDS[ri]);
    }
    for (int si = 0; si < 2; ++si) {
        const int s = si == 0 ? +1 : -1;
        for (int ri = 0; ri < 5; ++ri) emit_case(out, "edge", 24, s, RNDS[ri]);
    }
    for (int si = 0; si < 2; ++si) {
        const int s = si == 0 ? +1 : -1;
        for (int ri = 0; ri < 5; ++ri) emit_case(out, "edge", 113, s, RNDS[ri]);
    }
    /* adversarial: 12 -- the sign-dependent branches. */
    emit_case(out, "adversarial", 53, +1, MPFR_RNDA);
    emit_case(out, "adversarial", 53, +1, MPFR_RNDU);
    emit_case(out, "adversarial", 53, -1, MPFR_RNDZ);
    emit_case(out, "adversarial", 53, -1, MPFR_RNDD);
    emit_case(out, "adversarial", 200, +1, MPFR_RNDA);
    emit_case(out, "adversarial", 200, +1, MPFR_RNDU);
    emit_case(out, "adversarial", 200, -1, MPFR_RNDZ);
    emit_case(out, "adversarial", 200, -1, MPFR_RNDD);
    emit_case(out, "adversarial", 1, +1, MPFR_RNDA);
    emit_case(out, "adversarial", 1, +1, MPFR_RNDU);
    emit_case(out, "adversarial", 1, -1, MPFR_RNDZ);
    emit_case(out, "adversarial", 1, -1, MPFR_RNDD);
    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEECABBABEEEEULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 500);
            const int s = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            emit_case(out, "fuzz", prec, s, rnd);
        }
    }
    /* mined: 5 -- compound.c is called from compound_si; no isolated test. */
    emit_case(out, "mined", 53, +1, MPFR_RNDN);
    emit_case(out, "mined", 53, -1, MPFR_RNDN);
    emit_case(out, "mined", 53, +1, MPFR_RNDA);
    emit_case(out, "mined", 53, -1, MPFR_RNDZ);
    emit_case(out, "mined", 100, +1, MPFR_RNDU);
    return 0;
}
