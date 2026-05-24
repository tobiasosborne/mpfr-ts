/*
 * golden_driver.c -- Golden master for MPFR's mpfr_odd_p.
 *
 * Returns true iff x is an exact odd integer. mpfr/src/odd_p.c L25-L71.
 * The C source MPFR_ASSERTDs that the input is not singular; the TS port
 * relaxes this -- singular inputs return false (no MPFR-side assertion).
 *
 * IMPORTANT: because the C function asserts non-singular and assertions
 * may be active under MPFR's debug build, we DO NOT call mpfr_odd_p on
 * singular inputs in this driver. For singular inputs we emit cases that
 * the TS port handles internally (false for NaN/Inf/zero); since we
 * can't call libmpfr to verify, we omit them from the golden.
 *
 * Tag distribution: happy 22, edge 30, adversarial 10, fuzz 55, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_odd_p golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* mpfr_odd_p is exported from libmpfr (.so symbol) but not declared in
 * the public mpfr.h. Forward-declare so -Werror=implicit-decl doesn't
 * trip. Signature per mpfr/src/odd_p.c L25. */
int mpfr_odd_p(mpfr_srcptr y);

/* Emit only for non-singular x (regular). */
static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    /* mpfr_odd_p documented contract: non-singular only. We guard. */
    assert(mpfr_regular_p(x));
    const uint64_t t0 = now_ns();
    const int raw = mpfr_odd_p(x);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    jl_output_scalar_bool(out, raw);
    jl_finish(out, elapsed);
}

static inline void init_exact(mpfr_ptr x, uint64_t prec,
                              const char *m_dec, long e, int sign) {
    mpz_t z; mpz_init(z);
    if (mpz_set_str(z, m_dec, 10) != 0) { fprintf(stderr, "bad dec\n"); exit(2); }
    if (sign < 0) mpz_neg(z, z);
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_z_2exp(x, z, e, MPFR_RNDN);
    mpz_clear(z);
}

static inline void init_from_si(mpfr_ptr x, uint64_t prec, long v) {
    /* v != 0 required to keep regular_p true. */
    assert(v != 0);
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_si(x, v, MPFR_RNDN);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    /* d != 0 required; caller's responsibility. */
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}

#define EMIT(tag, init_expr) do { mpfr_t _x; init_expr; emit_case(out, tag, _x); mpfr_clear(_x); } while (0)

int main(void) {
    FILE *out = stdout;

    /* happy: 22 -- clear odd and even integers + non-integers. */
    EMIT("happy", init_from_si(_x, 53, 1));    /* odd */
    EMIT("happy", init_from_si(_x, 53, 3));    /* odd */
    EMIT("happy", init_from_si(_x, 53, 5));    /* odd */
    EMIT("happy", init_from_si(_x, 53, 7));    /* odd */
    EMIT("happy", init_from_si(_x, 53, 2));    /* even */
    EMIT("happy", init_from_si(_x, 53, 4));    /* even */
    EMIT("happy", init_from_si(_x, 53, 100));  /* even */
    EMIT("happy", init_from_si(_x, 53, 101));  /* odd */
    EMIT("happy", init_from_si(_x, 53, -1));   /* odd */
    EMIT("happy", init_from_si(_x, 53, -2));   /* even */
    EMIT("happy", init_from_si(_x, 53, -3));   /* odd */
    EMIT("happy", init_from_si(_x, 53, -100)); /* even */
    EMIT("happy", init_from_double(_x, 0.5, 53));   /* not integer -> not odd */
    EMIT("happy", init_from_double(_x, 1.5, 53));   /* not integer */
    EMIT("happy", init_from_double(_x, 3.14, 53));  /* not integer */
    EMIT("happy", init_from_double(_x, -0.25, 53)); /* not integer */
    EMIT("happy", init_from_si(_x, 53, 999999));   /* odd */
    EMIT("happy", init_from_si(_x, 53, 1000000));  /* even */
    EMIT("happy", init_from_si(_x, 64, 1234567));  /* odd */
    EMIT("happy", init_from_si(_x, 64, 1234568));  /* even */
    EMIT("happy", init_from_double(_x, 1e10, 53)); /* huge even integer */
    EMIT("happy", init_from_double(_x, 1e10 + 1.0, 53)); /* huge odd integer (if exact) */

    /* edge: 30 -- exp/prec boundaries. */
    /* exp <= 0: |x| < 1, not odd. */
    EMIT("edge", init_from_double(_x, 0.5, 53));
    EMIT("edge", init_from_double(_x, 0.25, 53));
    EMIT("edge", init_from_double(_x, 0.999, 53));
    EMIT("edge", init_from_double(_x, -0.5, 53));
    /* exp == prec: unit bit at LSB position. */
    EMIT("edge", init_exact(_x, 8, "128", 8, 1));   /* 1.0 -- odd */
    EMIT("edge", init_exact(_x, 8, "129", 8, 1));   /* 129 -- odd */
    EMIT("edge", init_exact(_x, 8, "130", 8, 1));   /* 130 -- even */
    EMIT("edge", init_exact(_x, 8, "192", 8, 1));   /* 192 -- even */
    EMIT("edge", init_exact(_x, 8, "255", 8, 1));   /* 255 -- odd */
    EMIT("edge", init_exact(_x, 8, "254", 8, 1));   /* 254 -- even */
    /* exp > prec: x is multiple of 2^(exp-prec) >= 2, so even (or not odd). */
    EMIT("edge", init_exact(_x, 8, "129", 9, 1));   /* 258 -- even */
    EMIT("edge", init_exact(_x, 8, "129", 16, 1));  /* large even */
    EMIT("edge", init_exact(_x, 8, "255", 100, 1)); /* very large even */
    /* 0 < exp < prec: unit bit position is interior; need it set AND fraction = 0. */
    EMIT("edge", init_exact(_x, 8, "192", 2, 1));   /* 0b11000000 with exp=2 -> 3 (odd) */
    EMIT("edge", init_exact(_x, 8, "128", 2, 1));   /* 0b10000000 with exp=2 -> 2 (even) */
    EMIT("edge", init_exact(_x, 8, "224", 3, 1));   /* 0b11100000 with exp=3 -> 7 (odd) */
    EMIT("edge", init_exact(_x, 8, "160", 2, 1));   /* 0b10100000 with exp=2 -> 2.5 (not integer) */
    EMIT("edge", init_exact(_x, 16, "32768", 1, 1));/* 1.0 (odd) */
    EMIT("edge", init_exact(_x, 16, "49152", 2, 1));/* 3.0 (odd) */
    EMIT("edge", init_exact(_x, 16, "40960", 2, 1));/* 2.5 (not integer) */
    EMIT("edge", init_exact(_x, 16, "32769", 16, 1)); /* exp=prec, unit bit set, fraction 0 -> odd */
    /* prec=1: only value is +/-1. */
    EMIT("edge", init_from_si(_x, 1, 1));
    EMIT("edge", init_from_si(_x, 1, -1));
    /* Large prec / large value. */
    EMIT("edge", init_from_si(_x, 256, 12345));
    EMIT("edge", init_from_si(_x, 256, 12346));
    EMIT("edge", init_from_si(_x, 4096, 1));
    EMIT("edge", init_from_si(_x, 4096, 2));
    /* exp == 1 (value in [1, 2)) -- only 1.0 is odd integer here. */
    EMIT("edge", init_from_double(_x, 1.5, 53));
    EMIT("edge", init_from_double(_x, 1.25, 53));

    /* adversarial: 10 -- close-to-odd and close-to-even cases. */
    EMIT("adversarial", init_exact(_x, 16, "32769", 16, 1));  /* unit bit set, fraction=0 -> odd */
    EMIT("adversarial", init_exact(_x, 16, "32770", 16, 1));  /* even */
    EMIT("adversarial", init_exact(_x, 16, "49153", 2, 1));   /* not integer (low bit at fractional) */
    EMIT("adversarial", init_exact(_x, 32, "2147483649", 32, 1));  /* odd */
    EMIT("adversarial", init_exact(_x, 32, "2147483648", 32, 1));  /* even */
    EMIT("adversarial", init_from_double(_x, 9007199254740993.0, 53)); /* 2^53+1 (may round) */
    EMIT("adversarial", init_from_double(_x, 9007199254740994.0, 53)); /* 2^53+2 even */
    EMIT("adversarial", init_exact(_x, 8, "129", 7, 1));     /* exp_ts=7, prec=8 -- non-integer */
    EMIT("adversarial", init_exact(_x, 8, "193", 8, 1));     /* odd, fraction zero */
    EMIT("adversarial", init_exact(_x, 8, "193", 8, -1));    /* -193 -- odd */

    /* fuzz: 55 -- integers and non-integers. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x0DDABBABBABBABBAULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 256);
            const uint64_t choice = xs64_below(&rng, 10);
            mpfr_t x;
            if (choice < 6) {
                /* mostly integers */
                long v = (long)((xs64_next(&rng) % 1000000ULL) + 1ULL);
                if (xs64_below(&rng, 2)) v = -v;
                init_from_si(x, prec, v);
            } else {
                /* non-integers */
                const uint64_t r1 = xs64_next(&rng);
                double d = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
                if (d == 0.0) d = 0.5;
                init_from_double(x, d, prec);
            }
            if (!mpfr_regular_p(x)) {
                mpfr_clear(x);
                continue;
            }
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
        }
    }

    /* mined: 5 */
    EMIT("mined", init_from_si(_x, 53, 1));
    EMIT("mined", init_from_si(_x, 53, 2));
    EMIT("mined", init_from_double(_x, 0.5, 53));
    EMIT("mined", init_from_si(_x, 53, -1));
    EMIT("mined", init_from_si(_x, 53, 17));

    return 0;
}
