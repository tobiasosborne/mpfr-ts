/*
 * golden_driver.c — Golden master for MPFR's mpfr_integer_p.
 *
 * Returns true iff x is an exact integer. mpfr/src/isinteger.c L25-L58.
 * Output is a bare JSON boolean.
 *
 * Tag distribution: happy 25, edge 35, adversarial 12, fuzz 60, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_integer_p golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    const uint64_t t0 = now_ns();
    const int raw = mpfr_integer_p(x);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    jl_output_scalar_bool(out, raw);
    jl_finish(out, elapsed);
}

/* Build x = m * 2^e exactly via mpfr_set_z_2exp. m_dec gives the integer
 * mantissa as a decimal string; e is the binary exponent. The result is
 * MSB-aligned to the given prec. */
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
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_si(x, v, MPFR_RNDN);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}

static inline void init_pos_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1); }
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1); }
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1); }
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1); }
static inline void init_nan(mpfr_ptr x, uint64_t prec)      { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_nan(x); }

#define EMIT(tag, init_expr) do { mpfr_t _x; init_expr; emit_case(out, tag, _x); mpfr_clear(_x); } while (0)

int main(void) {
    FILE *out = stdout;

    /* happy: 25 — clear integers + clear non-integers. */
    EMIT("happy", init_from_si(_x, 53, 0));       /* 0 is integer */
    EMIT("happy", init_from_si(_x, 53, 1));
    EMIT("happy", init_from_si(_x, 53, -1));
    EMIT("happy", init_from_si(_x, 53, 2));
    EMIT("happy", init_from_si(_x, 53, 100));
    EMIT("happy", init_from_si(_x, 53, -1234567));
    EMIT("happy", init_from_double(_x, 0.5, 53));    /* not integer */
    EMIT("happy", init_from_double(_x, 1.5, 53));    /* not integer */
    EMIT("happy", init_from_double(_x, -0.25, 53));  /* not integer */
    EMIT("happy", init_from_double(_x, 3.14, 53));   /* not integer */
    EMIT("happy", init_from_double(_x, 1e10, 53));   /* integer */
    EMIT("happy", init_from_double(_x, 1e-10, 53));  /* not integer */
    EMIT("happy", init_from_double(_x, 2.0, 53));    /* integer */
    EMIT("happy", init_from_double(_x, 1.0, 53));    /* integer */
    EMIT("happy", init_from_double(_x, -1.0, 53));   /* integer */
    EMIT("happy", init_from_double(_x, 4.0, 53));    /* integer */
    EMIT("happy", init_from_double(_x, 8.0, 53));    /* integer */
    EMIT("happy", init_from_double(_x, -8.0, 53));   /* integer */
    EMIT("happy", init_from_double(_x, 0.125, 53));  /* not integer */
    EMIT("happy", init_pos_zero(_x, 53));            /* integer */
    EMIT("happy", init_neg_zero(_x, 53));            /* integer */
    EMIT("happy", init_pos_inf(_x, 53));             /* not integer */
    EMIT("happy", init_neg_inf(_x, 53));             /* not integer */
    EMIT("happy", init_nan(_x, 53));                 /* not integer */
    EMIT("happy", init_from_si(_x, 64, 1234567890));

    /* edge: 35 — boundaries on exp vs prec, near-integers. */
    /* exp = 0: value in [0.5, 1) — not integer. */
    EMIT("edge", init_from_double(_x, 0.5, 53));
    EMIT("edge", init_from_double(_x, 0.999, 53));
    /* exp = 1, value in [1, 2). The only integer in this range is 1. */
    EMIT("edge", init_from_double(_x, 1.0, 53));
    EMIT("edge", init_from_double(_x, 1.5, 53));
    EMIT("edge", init_from_double(_x, 1.999, 53));
    /* exp = prec: every value is an integer (the fractional bits are zero
     * by structure). */
    EMIT("edge", init_exact(_x, 8, "128", 8, 1));    /* 128 = 1.0 * 2^7, exp_ts=8 */
    EMIT("edge", init_exact(_x, 8, "255", 8, 1));    /* 255 */
    EMIT("edge", init_exact(_x, 8, "129", 8, 1));    /* 129 */
    /* exp > prec: x is a multiple of 2^(exp-prec), so always integer. */
    EMIT("edge", init_exact(_x, 8, "128", 16, 1));   /* 128 << 8 = 32768 */
    EMIT("edge", init_exact(_x, 8, "129", 100, 1));  /* very large */
    /* 0 < exp < prec: the tricky case. */
    EMIT("edge", init_exact(_x, 8, "192", 2, 1));    /* 192 = 11000000, exp_ts=2 → value = 3.0 (integer) */
    EMIT("edge", init_exact(_x, 8, "160", 2, 1));    /* 160 = 10100000, exp_ts=2 → value = 2.5 (not integer) */
    EMIT("edge", init_exact(_x, 8, "129", 2, 1));    /* 129 = 10000001, exp_ts=2 → value = 2 + 1/64 (not integer) */
    /* All singular kinds. */
    EMIT("edge", init_nan(_x, 1));
    EMIT("edge", init_nan(_x, 100));
    EMIT("edge", init_pos_inf(_x, 1));
    EMIT("edge", init_pos_inf(_x, 100));
    EMIT("edge", init_neg_inf(_x, 1));
    EMIT("edge", init_neg_inf(_x, 100));
    EMIT("edge", init_pos_zero(_x, 1));
    EMIT("edge", init_neg_zero(_x, 1));
    EMIT("edge", init_pos_zero(_x, 4096));
    EMIT("edge", init_neg_zero(_x, 4096));
    /* prec=1, integer 1. */
    EMIT("edge", init_from_si(_x, 1, 1));
    EMIT("edge", init_from_si(_x, 1, -1));
    /* Large exp boundaries. */
    EMIT("edge", init_from_double(_x, 1e15, 53));    /* integer */
    EMIT("edge", init_from_double(_x, 1e15 + 0.5, 53));  /* may round; check */
    EMIT("edge", init_from_double(_x, 1e17, 53));    /* integer */
    /* Negative integers. */
    EMIT("edge", init_from_si(_x, 64, -100));
    EMIT("edge", init_from_si(_x, 64, -1));
    /* Just-under-half. */
    EMIT("edge", init_from_double(_x, 1.5 - 1e-15, 53));
    EMIT("edge", init_from_double(_x, 2.0 - 1e-15, 53));
    EMIT("edge", init_from_double(_x, 0.1, 53));
    EMIT("edge", init_from_double(_x, 0.25, 53));

    /* adversarial: 12 — close-to-integer cases. */
    EMIT("adversarial", init_exact(_x, 16, "32768", 16, 1));     /* exact power 2 — integer */
    EMIT("adversarial", init_exact(_x, 16, "32769", 16, 1));     /* not integer (low bit set at fractional position)? exp_ts=16, prec=16, exp >= prec → integer */
    EMIT("adversarial", init_exact(_x, 16, "49152", 2, 1));      /* 49152 = 0b1100000000000000 = 3 * 2^14; exp_ts=2 → 3 * 2^14 / 2^14 = 3 (integer) */
    EMIT("adversarial", init_exact(_x, 16, "40960", 2, 1));      /* 40960 = 0b1010000000000000 = 5 * 2^13; exp_ts=2 → 2.5 (not integer) */
    EMIT("adversarial", init_exact(_x, 16, "32769", 2, 1));      /* very-close-to-integer (low bit) → not integer */
    EMIT("adversarial", init_from_double(_x, 1024.5, 53));
    EMIT("adversarial", init_from_double(_x, 1024.0, 53));
    EMIT("adversarial", init_from_double(_x, 1024.0 + 1.0/(double)(1ULL<<40), 53));
    EMIT("adversarial", init_exact(_x, 64, "9223372036854775808", 64, 1));  /* 2^63 — integer (exp=64 >= prec=64 case-wise, exp_ts=64) */
    EMIT("adversarial", init_exact(_x, 64, "9223372036854775808", 1, 1));   /* exp_ts=1 → 1.0 (integer) */
    EMIT("adversarial", init_exact(_x, 64, "9223372036854775809", 63, 1));  /* low bit set at fractional → not integer (exp_ts=63 < prec=64) */
    EMIT("adversarial", init_exact(_x, 64, "13835058055282163712", 64, 1)); /* 3 * 2^62, exp=64 → integer */

    /* fuzz: 60 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x1234567890ABCDEFULL);
        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 256);
            const uint64_t choice = xs64_below(&rng, 10);
            mpfr_t x;
            if (choice < 6) {
                /* mostly normal */
                const long v = (long)xs64_next(&rng) % 1000000;
                init_from_si(x, prec, v);
            } else if (choice < 8) {
                /* random non-integer doubles */
                const uint64_t r1 = xs64_next(&rng);
                double d = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
                init_from_double(x, d, prec);
            } else if (choice < 9) {
                init_pos_zero(x, prec);
            } else {
                init_pos_inf(x, prec);
            }
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
        }
    }

    /* mined: 5 — integer/non-integer mix common in MPFR test suite. */
    EMIT("mined", init_from_si(_x, 53, 0));
    EMIT("mined", init_from_si(_x, 53, 17));
    EMIT("mined", init_from_double(_x, 0.5, 53));
    EMIT("mined", init_pos_inf(_x, 53));
    EMIT("mined", init_nan(_x, 53));

    return 0;
}
