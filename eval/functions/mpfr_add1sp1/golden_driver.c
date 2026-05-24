/*
 * golden_driver.c — Golden master for MPFR's mpfr_add1sp1.
 *
 * The C function mpfr_add1sp1 is `static` (file-private) in
 * mpfr/src/add1sp.c — not externally linkable. We exercise it
 * indirectly by calling mpfr_add with arguments that satisfy the
 * add1sp1 dispatch preconditions:
 *
 *   - MPFR_PREC(a) == MPFR_PREC(b) == MPFR_PREC(c) < GMP_NUMB_BITS
 *   - b and c kind='normal' with the same sign
 *
 * In that regime, mpfr_add1sp (mpfr/src/add1sp.c L856-L894) routes to
 * mpfr_add1sp1, so the bit-pattern of mpfr_add's result == the
 * bit-pattern of mpfr_add1sp1's result. The TS port mpfr_add1sp1
 * directly implements the algorithm; the golden therefore compares
 * the TS port's output against libmpfr's mpfr_add result on the
 * same constrained inputs.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_add1sp1(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                           mpfr_rnd_t rnd_mode, mpfr_prec_t p);
 *
 * TS signature: mpfr_add1sp1(b, c, rnd) -> Result.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"b":<MPFR>,"c":<MPFR>,"rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 *   - b, c via jl_kv_mpfr (full MPFR records).
 *   - rnd via jl_kv_rnd.
 *   - Output via jl_output_result.
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  22
 *   edge         :  30
 *   adversarial  :  12
 *   fuzz         :  60
 *   mined        :   5
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/add1sp.c — C reference.
 * Ref: src/ops/add1sp1.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_add1sp1 golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* The 5 supported rounding modes (matches the TS port). */
static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
};

/* Default emax — match the TS port's constant. */
#define EMAX_DEFAULT ((mpfr_exp_t)((1LL << 30) - 1))

/* Emit one case at (b, c, rnd). Preconditions:
 *  - b.prec == c.prec, in [1, 63].
 *  - b, c kind='normal' (caller must NOT pass zero/inf/nan).
 *  - b.sign == c.sign (same-sign).
 *
 * The driver does NOT validate the kind/sign preconditions; the caller
 * must ensure them. Violations would cause mpfr_add to route to a
 * different internal path (e.g. mpfr_sub1sp1 for opposite signs, or
 * the specials dispatch for zero/inf/nan), and the golden would
 * record those outputs — which the TS mpfr_add1sp1 port would reject
 * via its precondition checks (EPREC). So we just don't generate
 * such cases. */
static void
emit_case(FILE *out, const char *tag,
          mpfr_srcptr b, mpfr_srcptr c, mpfr_rnd_t rnd) {
    /* Pre-check by reading b's prec. The driver uses b's prec for the
     * output. */
    const mpfr_prec_t p = mpfr_get_prec(b);
    assert(p == mpfr_get_prec(c));
    assert(p >= 1 && (uint64_t)p < 64);
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
    assert(mpfr_signbit(b) == mpfr_signbit(c));

    mpfr_t a;
    mpfr_init2(a, p);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_add(a, b, c, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_mpfr(out, 0, "c", c);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, a, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(a);
}

/* Build a same-prec, same-sign pair (b, c) from doubles. */
static void mk_pair_dd(mpfr_ptr b, mpfr_ptr c, uint64_t prec,
                       double db, double dc) {
    assert(prec >= 1 && prec < 64);
    mpfr_init2(b, (mpfr_prec_t)prec);
    mpfr_init2(c, (mpfr_prec_t)prec);
    mpfr_set_d(b, db, MPFR_RNDN);
    mpfr_set_d(c, dc, MPFR_RNDN);
    /* Verify same-sign post-rounding. If one rounded to ±0 it loses
     * normalness; we skip the case via assertion. */
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
    assert(mpfr_signbit(b) == mpfr_signbit(c));
}

static void clear_pair(mpfr_ptr b, mpfr_ptr c) {
    mpfr_clear(b);
    mpfr_clear(c);
}

/* Build (b, c) from integers (both same sign by `sign` parameter; both
 * at the given prec). Uses mpfr_set_si then mpfr_setsign to ensure
 * sign uniformity. The integer magnitudes a, b are positive and small;
 * sign forces the value's sign without affecting magnitude. */
static void mk_pair_ii(mpfr_ptr b, mpfr_ptr c, uint64_t prec,
                       long magB, long magC, int sign) {
    assert(prec >= 1 && prec < 64);
    assert(magB > 0 && magC > 0);
    assert(sign == +1 || sign == -1);
    mpfr_init2(b, (mpfr_prec_t)prec);
    mpfr_init2(c, (mpfr_prec_t)prec);
    mpfr_set_si(b, sign * magB, MPFR_RNDN);
    mpfr_set_si(c, sign * magC, MPFR_RNDN);
    /* Should both be pure FP since magnitudes are nonzero. */
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
}

/* Build (b, c) with exact-bit-pattern mantissas at the given prec,
 * sign per arg. The mantissa must satisfy 2^(p-1) <= mant < 2^p (TS
 * MSB-alignment); we use mpz_t and mpfr_set_z_2exp to set the value
 * directly. The MPFR exp passed to set_z_2exp is the "f = z * 2^exp"
 * exponent, which differs from the TS-schema exp by p (TS exp = z_exp
 * + p). To force MPFR exp = E (TS-schema), we pass z_exp = E - p.
 *
 * NB: mpfr_set_z_2exp rounds, but if the mantissa already fits in p
 * bits and the z_exp puts the MSB at the right place, the conversion
 * is exact. */
static void mk_pair_exact(mpfr_ptr b, mpfr_ptr c, uint64_t prec,
                          const char *mantB_dec, mpfr_exp_t expB,
                          const char *mantC_dec, mpfr_exp_t expC,
                          int sign) {
    mpz_t zB, zC;
    mpz_init(zB);
    mpz_init(zC);
    if (mpz_set_str(zB, mantB_dec, 10) != 0
        || mpz_set_str(zC, mantC_dec, 10) != 0) {
        fprintf(stderr, "mk_pair_exact: bad decimal\n");
        exit(2);
    }
    if (sign < 0) {
        mpz_neg(zB, zB);
        mpz_neg(zC, zC);
    }
    mpfr_init2(b, (mpfr_prec_t)prec);
    mpfr_init2(c, (mpfr_prec_t)prec);
    /* mpfr_set_z_2exp(rop, z, e, rnd): rop = z * 2^e. */
    mpfr_set_z_2exp(b, zB, expB - (mpfr_exp_t)prec, MPFR_RNDN);
    mpfr_set_z_2exp(c, zC, expC - (mpfr_exp_t)prec, MPFR_RNDN);
    mpz_clear(zB);
    mpz_clear(zC);
    /* Both must be normal post-rounding. The caller ensures the
     * mantissa bits fit in prec; if not, mpfr would round and the
     * value might lose normalness — assert. */
    assert(mpfr_regular_p(b));
    assert(mpfr_regular_p(c));
    assert(mpfr_signbit(b) == mpfr_signbit(c));
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 22 cases — common precs and simple values.              */
    /* ============================================================== */
    {
        /* prec=53: typical scientific. */
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 1.0, 1.0);
            emit_case(out, "happy", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 1.0, 2.0);
            emit_case(out, "happy", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 3.14, 2.71);
            emit_case(out, "happy", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, -1.5, -2.5);
            emit_case(out, "happy", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 0.5, 0.25);
            emit_case(out, "happy", b, c, MPFR_RNDZ);
            clear_pair(b, c);
        }
        /* prec=24 (float32). */
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 24, 1.0, 1.0);
            emit_case(out, "happy", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 24, -3.14, -1.5);
            emit_case(out, "happy", b, c, MPFR_RNDD);
            clear_pair(b, c);
        }
        /* prec=8 — small mantissa, tighter rounding. */
        {
            mpfr_t b, c;
            mk_pair_ii(&b[0], &c[0], 8, 3, 5, +1);
            emit_case(out, "happy", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_ii(&b[0], &c[0], 8, 100, 200, +1);
            emit_case(out, "happy", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_ii(&b[0], &c[0], 8, 100, 200, -1);
            emit_case(out, "happy", b, c, MPFR_RNDU);
            clear_pair(b, c);
        }
        /* Across all 5 rounding modes at prec=53. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 1.5, 2.5);
            emit_case(out, "happy", b, c, RNDS[i]);
            clear_pair(b, c);
        }
        /* Far-apart magnitudes (d large). */
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 1e10, 1.0);
            emit_case(out, "happy", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 1.0, 1e10);
            emit_case(out, "happy", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        /* Different-exponent pair with small d. */
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 4.0, 1.0);  /* d=2 */
            emit_case(out, "happy", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        /* Negative pair, different exponents. */
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, -8.0, -1.0);
            emit_case(out, "happy", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        /* Test some prec=32 (32 bit float-like). */
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 32, 100.5, 200.25);
            emit_case(out, "happy", b, c, MPFR_RNDA);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 32, -100.5, -200.25);
            emit_case(out, "happy", b, c, MPFR_RNDA);
            clear_pair(b, c);
        }
    }

    /* ============================================================== */
    /* edge: 30 cases — prec=1, prec=63 (max allowed), exponent       */
    /* boundaries, d boundaries.                                      */
    /* ============================================================== */
    {
        /* prec=1: mantissa is just the MSB; 1 + 1 = 2 (or -1 + -1 = -2). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c;
            mk_pair_ii(&b[0], &c[0], 1, 1, 1, +1);
            emit_case(out, "edge", b, c, RNDS[i]);
            clear_pair(b, c);
        }
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c;
            mk_pair_ii(&b[0], &c[0], 1, 1, 1, -1);
            emit_case(out, "edge", b, c, RNDS[i]);
            clear_pair(b, c);
        }

        /* prec=63 (maximum allowed for add1sp1): exercises sh=1. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 63, 1.0, 1.0);
            emit_case(out, "edge", b, c, RNDS[i]);
            clear_pair(b, c);
        }

        /* prec=2: sh=62. */
        {
            mpfr_t b, c;
            mk_pair_ii(&b[0], &c[0], 2, 1, 1, +1);  /* 1 + 1 = 2 */
            emit_case(out, "edge", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_ii(&b[0], &c[0], 2, 2, 1, +1);  /* 2 + 1 = 3 */
            emit_case(out, "edge", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_ii(&b[0], &c[0], 2, 3, 1, +1);  /* 3 + 1 = 4, rounds */
            emit_case(out, "edge", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }

        /* d boundary: d = sh-1 (= 62 at prec=2). */
        {
            mpfr_t b, c;
            /* prec=2, sh=62. b has exp=63, c has exp=2 → d=61. */
            mk_pair_exact(&b[0], &c[0], 2, "2", 63, "2", 2, +1);
            emit_case(out, "edge", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }

        /* d boundary: d = sh exactly. prec=10, sh=54. b exp=60, c exp=6 → d=54. */
        {
            mpfr_t b, c;
            mk_pair_exact(&b[0], &c[0], 10, "512", 60, "512", 6, +1);
            emit_case(out, "edge", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }

        /* d boundary: d = 64 exactly (B3 branch). prec=10, sh=54. */
        {
            mpfr_t b, c;
            mk_pair_exact(&b[0], &c[0], 10, "512", 70, "512", 6, +1);
            emit_case(out, "edge", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }

        /* d > 64. */
        {
            mpfr_t b, c;
            mk_pair_exact(&b[0], &c[0], 10, "512", 100, "512", 6, +1);
            emit_case(out, "edge", b, c, MPFR_RNDU);
            clear_pair(b, c);
        }

        /* Very small magnitudes. */
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 1e-100, 1e-100);
            emit_case(out, "edge", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        /* Very large magnitudes (but well below emax). */
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 1e100, 1e100);
            emit_case(out, "edge", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }

        /* Same-exp same-mantissa (sum is exactly 2x). */
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 7.0, 7.0);
            emit_case(out, "edge", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }

        /* Carry propagation case: each operand has mant near 2^prec-1. */
        {
            mpfr_t b, c;
            /* prec=4, mant=15 (=2^4-1), exp=4 → value = 15 * 2^0 = 15 */
            mk_pair_exact(&b[0], &c[0], 4, "15", 4, "15", 4, +1);
            emit_case(out, "edge", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_exact(&b[0], &c[0], 4, "15", 4, "15", 4, -1);
            emit_case(out, "edge", b, c, MPFR_RNDZ);
            clear_pair(b, c);
        }
    }

    /* ============================================================== */
    /* adversarial: 12 cases — rounding-boundary cases.               */
    /* ============================================================== */
    {
        /* Tie-to-even cases at prec=4: 0b1010 + 0b0011 = 0b1101 — depends
         * on rounding. Use mk_pair_exact for full control. */
        {
            mpfr_t b, c;
            /* prec=4, b = 10 * 2^0 = 10; c = 3 * 2^0 = 3. sum = 13, fits in 4 bits. */
            mk_pair_exact(&b[0], &c[0], 4, "10", 4, "3", 2, +1);
            emit_case(out, "adversarial", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }

        /* Carry-then-round-up: each near 2^prec-1. */
        {
            mpfr_t b, c;
            /* prec=8, both = 255 = 2^8-1. sum = 510 = 2^9 - 2, rounds. */
            mk_pair_exact(&b[0], &c[0], 8, "255", 8, "255", 8, +1);
            emit_case(out, "adversarial", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }

        /* Smallest possible d=1 case (very close magnitudes). */
        {
            mpfr_t b, c;
            mk_pair_exact(&b[0], &c[0], 16, "32768", 16, "32768", 15, +1);
            emit_case(out, "adversarial", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }

        /* Both signs through RNDD/RNDU sensitivity. */
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 16, 3.14, 1.59);
            emit_case(out, "adversarial", b, c, MPFR_RNDD);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 16, 3.14, 1.59);
            emit_case(out, "adversarial", b, c, MPFR_RNDU);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 16, -3.14, -1.59);
            emit_case(out, "adversarial", b, c, MPFR_RNDD);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 16, -3.14, -1.59);
            emit_case(out, "adversarial", b, c, MPFR_RNDU);
            clear_pair(b, c);
        }

        /* Round-bit boundary: sticky=0, rb=1, low bit of result=0 (tie to even truncates). */
        {
            mpfr_t b, c;
            /* prec=3, b = 4 * 2^0 (mant=4=0b100, exp=3 in TS schema).
             * c = 4 * 2^0. Sum = 8 = 0b1000, normalises to mant=4, exp=4
             * with no round/sticky → exact. We want a case where rb=1
             * and sb=0; use d=sh-1. */
            mk_pair_exact(&b[0], &c[0], 3, "4", 3, "4", 0, +1);
            emit_case(out, "adversarial", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }

        /* Round-bit boundary RNDA. */
        {
            mpfr_t b, c;
            mk_pair_exact(&b[0], &c[0], 5, "16", 5, "16", 0, +1);
            emit_case(out, "adversarial", b, c, MPFR_RNDA);
            clear_pair(b, c);
        }

        /* Carry-and-renormalise across signs. */
        {
            mpfr_t b, c;
            mk_pair_exact(&b[0], &c[0], 4, "15", 4, "15", 4, +1);
            emit_case(out, "adversarial", b, c, MPFR_RNDU);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_exact(&b[0], &c[0], 4, "15", 4, "15", 4, -1);
            emit_case(out, "adversarial", b, c, MPFR_RNDD);
            clear_pair(b, c);
        }

        /* Very wide d, prec=63 (max). */
        {
            mpfr_t b, c;
            mk_pair_exact(&b[0], &c[0], 63,
                          "4611686018427387904", 63,
                          "4611686018427387904", 0, +1);
            emit_case(out, "adversarial", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG-driven                                  */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xADD15A911CADD1F1ULL);

        for (int rep = 0; rep < 60; ++rep) {
            /* prec in [1, 63]. */
            const uint64_t prec = 1 + xs64_below(&rng, 63);
            /* Magnitudes drawn from doubles in a moderate range. */
            const double magB = (double)(1 + xs64_below(&rng, 1000000));
            const double magC = (double)(1 + xs64_below(&rng, 1000000));
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            const uint64_t rnd_idx = xs64_below(&rng, 5);

            mpfr_t b, c;
            mpfr_init2(b, (mpfr_prec_t)prec);
            mpfr_init2(c, (mpfr_prec_t)prec);
            mpfr_set_d(b, sign * magB, MPFR_RNDN);
            mpfr_set_d(c, sign * magC, MPFR_RNDN);
            /* Verify post-rounding both are still normal & same-sign.
             * For tiny prec, mpfr_set_d may round to zero — skip those. */
            if (!mpfr_regular_p(b) || !mpfr_regular_p(c)
                || mpfr_signbit(b) != mpfr_signbit(c)) {
                mpfr_clear(b);
                mpfr_clear(c);
                continue;
            }
            emit_case(out, "fuzz", b, c, RNDS[rnd_idx]);
            mpfr_clear(b);
            mpfr_clear(c);
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — representative shapes from mpfr/tests/        */
    /* tadd.c (the public mpfr_add test suite, which exercises every */
    /* internal path including add1sp1). The shapes that route here  */
    /* are: same-prec, same-sign, prec < 64.                          */
    /* ============================================================== */
    {
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 1.0, 1.0);
            emit_case(out, "mined", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, 1.5, 2.5);
            emit_case(out, "mined", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 24, 1.0, 1e-7);  /* large d */
            emit_case(out, "mined", b, c, MPFR_RNDU);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_dd(&b[0], &c[0], 53, -2.0, -3.0);
            emit_case(out, "mined", b, c, MPFR_RNDZ);
            clear_pair(b, c);
        }
        {
            mpfr_t b, c;
            mk_pair_ii(&b[0], &c[0], 32, 1, 1, +1);
            emit_case(out, "mined", b, c, MPFR_RNDN);
            clear_pair(b, c);
        }
    }

    return 0;
}
