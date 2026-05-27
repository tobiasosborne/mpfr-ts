/*
 * golden_driver.c -- Golden master for MPFR's mpfr_print_mant_binary.
 *
 * C: void mpfr_print_mant_binary(const char *str, const mp_limb_t *p, mpfr_prec_t r).
 *    Ref: mpfr/src/print_raw.c L26-L47.
 *
 * The C function printf's to STDOUT directly. To capture it for the
 * golden, we redirect stdout to a tmpfile, call the function, then
 * read the tmpfile back. The captured string is the wire output.
 *
 * Inputs differ from the C body: the wire records (str, x) where x is
 * an MPFR value -- the TS port extracts limbs from x.mant, mirroring
 * the C side's raw limb layout.
 *
 * Wire: {"inputs":{"str":"<prefix>","x":<mpfr>},"output":"<formatted>"}.
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_print_mant_binary golden_driver requires GMP_NUMB_BITS == 64"
#endif

extern void mpfr_print_mant_binary(const char *str, const mp_limb_t *p, mpfr_prec_t r);

/* Internal MPFR fields/helpers not in the public mpfr.h. We mirror the
 * mpfr-impl.h definitions inline so the driver compiles against just
 * libmpfr.so. The struct layout is stable across 4.x. */
#define DRV_MANT(x) ((mp_limb_t*)((x)->_mpfr_d))
#define DRV_PREC(x) ((x)->_mpfr_prec)

/* mpfr_setmax(x, e) sets x to the largest finite value with exponent e:
 * 2^e * (1 - 2^-prec). Public-API equivalent: set to 2^e then step down. */
static inline void drv_setmax(mpfr_t x, mpfr_exp_t e) {
    mpfr_set_ui_2exp(x, 1u, e, MPFR_RNDN);
    mpfr_nextbelow(x);
}
#define mpfr_setmax(x, e) drv_setmax((x), (e))

/* Capture stdout output of mpfr_print_mant_binary into buf. */
static inline void capture_print(char *buf, size_t bufsz,
                                 const char *prefix, mpfr_srcptr x) {
    /* Save the current stdout fd; redirect to a tmpfile. */
    fflush(stdout);
    int saved_stdout = dup(fileno(stdout));
    if (saved_stdout < 0) { fprintf(stderr, "dup stdout failed\n"); exit(2); }
    FILE *tmp = tmpfile();
    if (!tmp) { fprintf(stderr, "tmpfile failed\n"); exit(2); }
    if (dup2(fileno(tmp), fileno(stdout)) < 0) {
        fprintf(stderr, "dup2 failed\n"); exit(2);
    }
    /* Call the function -- writes to (redirected) stdout. */
    mpfr_print_mant_binary(prefix, DRV_MANT(x), DRV_PREC(x));
    fflush(stdout);
    /* Restore stdout, then read the tmpfile. */
    fflush(stdout);
    dup2(saved_stdout, fileno(stdout));
    close(saved_stdout);
    rewind(tmp);
    size_t n = fread(buf, 1, bufsz - 1, tmp);
    buf[n] = '\0';
    fclose(tmp);
}

static inline void emit_case(FILE *out, const char *tag,
                             const char *prefix, mpfr_srcptr x) {
    /* Only call on normal values (the function reads mant directly;
     * for singulars there is no meaningful mantissa). */
    assert(mpfr_regular_p(x));
    char buf[8192];
    const uint64_t t0 = now_ns();
    capture_print(buf, sizeof buf, prefix, x);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_str(out, 1, "str", prefix);
    jl_kv_mpfr(out, 0, "x", x);
    jl_end_inputs(out);
    jl_output_scalar_str(out, buf);
    jl_finish(out, elapsed);
}

static inline void emit_d(FILE *out, const char *tag, const char *prefix,
                          double d, uint64_t prec) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
    if (mpfr_regular_p(x)) {
        emit_case(out, tag, prefix, x);
    }
    mpfr_clear(x);
}

static inline void emit_pow2(FILE *out, const char *tag, const char *prefix,
                             uint64_t prec, mpfr_exp_t e) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_ui(x, 1, MPFR_RNDN); mpfr_set_exp(x, e);
    emit_case(out, tag, prefix, x);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- normal values with various precisions and prefixes. */
    emit_d(out, "happy", "x", 3.14, 53);
    emit_d(out, "happy", "x", -3.14, 53);
    emit_d(out, "happy", "x", 2.71828, 53);
    emit_d(out, "happy", "x", -2.71828, 53);
    emit_d(out, "happy", "x", 1.0, 53);
    emit_d(out, "happy", "x", -1.0, 53);
    emit_d(out, "happy", "x", 0.5, 53);
    emit_d(out, "happy", "x", 100.5, 53);
    emit_d(out, "happy", "x", 1e-10, 53);
    emit_d(out, "happy", "x", 1e10, 53);
    /* Various precisions. */
    emit_d(out, "happy", "y", 3.14, 24);
    emit_d(out, "happy", "y", 3.14, 64);
    emit_d(out, "happy", "y", 3.14, 100);
    emit_d(out, "happy", "y", 3.14, 128);
    emit_d(out, "happy", "y", 3.14, 200);
    /* Various prefixes (test the str arg threading). */
    emit_d(out, "happy", "DEBUG", 1.0, 53);
    emit_d(out, "happy", "tmp_mant", 1.0, 53);
    emit_d(out, "happy", "[trace]", 1.0, 53);
    emit_d(out, "happy", "", 1.0, 53);  /* empty prefix */
    emit_d(out, "happy", " ", 1.0, 53);

    /* edge: 30 -- prec on/near limb boundaries, MSB-only / all-ones mantissas. */
    /* prec=1 */
    emit_d(out, "edge", "x", 1.0, 1);
    emit_d(out, "edge", "x", -1.0, 1);
    emit_d(out, "edge", "x", 2.0, 1);
    emit_d(out, "edge", "x", 0.5, 1);
    /* prec=2 */
    emit_d(out, "edge", "x", 3.0, 2);
    emit_d(out, "edge", "x", -3.0, 2);
    /* prec at limb boundaries */
    emit_d(out, "edge", "x", 1.5, 63);  /* 1 limb */
    emit_d(out, "edge", "x", 1.5, 64);  /* exactly 1 limb */
    emit_d(out, "edge", "x", 1.5, 65);  /* 2 limbs */
    emit_d(out, "edge", "x", 1.5, 127);  /* 2 limbs */
    emit_d(out, "edge", "x", 1.5, 128);  /* exactly 2 limbs */
    emit_d(out, "edge", "x", 1.5, 129);  /* 3 limbs */
    emit_d(out, "edge", "x", 1.5, 192);  /* exactly 3 limbs */
    emit_d(out, "edge", "x", 1.5, 193);  /* 4 limbs */
    emit_d(out, "edge", "x", 1.5, 256);  /* exactly 4 limbs */
    /* Pow2 (MSB-only mantissa) across precisions. */
    emit_pow2(out, "edge", "x", 1, 0);
    emit_pow2(out, "edge", "x", 53, 0);
    emit_pow2(out, "edge", "x", 64, 0);
    emit_pow2(out, "edge", "x", 65, 0);
    emit_pow2(out, "edge", "x", 128, 0);
    emit_pow2(out, "edge", "x", 129, 0);
    /* All-ones mantissa across precisions. */
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_setmax(x, 10);
        emit_case(out, "edge", "x", x);
        mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 64); mpfr_setmax(x, 10);
        emit_case(out, "edge", "x", x);
        mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 65); mpfr_setmax(x, 10);
        emit_case(out, "edge", "x", x);
        mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 128); mpfr_setmax(x, 10);
        emit_case(out, "edge", "x", x);
        mpfr_clear(x);
    }
    /* Negative pow2 */
    {
        mpfr_t x; mpfr_init2(x, 53);
        mpfr_set_ui(x, 1, MPFR_RNDN); mpfr_set_exp(x, 5);
        mpfr_neg(x, x, MPFR_RNDN);
        emit_case(out, "edge", "x", x);
        mpfr_clear(x);
    }
    /* Special prefixes with quote / backslash to verify JSON escaping. */
    emit_d(out, "edge", "with \"quotes\"", 1.0, 53);
    emit_d(out, "edge", "with\\backslash", 1.0, 53);
    emit_d(out, "edge", "with\ttab", 1.0, 53);
    emit_d(out, "edge", "x", 1.0, 320);  /* 5-limb mantissa boundary */

    /* adversarial: 12 -- specific bit patterns at limb boundaries. */
    {
        /* Mantissa with only the LSB and MSB set at prec=53. */
        mpfr_t x; mpfr_init2(x, 53);
        mpfr_set_str(x, "1.0000000000000000000000000000000000000000000000000001", 2, MPFR_RNDN);
        emit_case(out, "adversarial", "x", x);
        mpfr_clear(x);
    }
    {
        /* Mantissa with alternating bits at prec=64. */
        mpfr_t x; mpfr_init2(x, 64);
        mpfr_set_str(x, "1.010101010101010101010101010101010101010101010101010101010101010", 2, MPFR_RNDN);
        emit_case(out, "adversarial", "x", x);
        mpfr_clear(x);
    }
    {
        /* Mantissa with all ones at prec=64. */
        mpfr_t x; mpfr_init2(x, 64); mpfr_setmax(x, 0);
        emit_case(out, "adversarial", "x", x);
        mpfr_clear(x);
    }
    {
        /* Mantissa with all ones at prec=128 (2 limbs). */
        mpfr_t x; mpfr_init2(x, 128); mpfr_setmax(x, 0);
        emit_case(out, "adversarial", "x", x);
        mpfr_clear(x);
    }
    /* Single-bit-set at various positions. */
    emit_pow2(out, "adversarial", "x", 100, 50);
    emit_pow2(out, "adversarial", "x", 100, -50);
    emit_pow2(out, "adversarial", "x", 100, 0);
    emit_pow2(out, "adversarial", "x", 200, 100);
    /* Boundary precisions (just above/below limb counts). */
    emit_d(out, "adversarial", "x", 1.0, 65);
    emit_d(out, "adversarial", "x", 1.0, 127);
    emit_d(out, "adversarial", "x", 1.0, 129);
    /* Negative all-ones */
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_setmax(x, 0); mpfr_neg(x, x, MPFR_RNDN);
        emit_case(out, "adversarial", "x", x);
        mpfr_clear(x);
    }

    /* fuzz: 50 random normal values across precisions and prefixes. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x1234CAFEBABE5678ULL);
        const uint64_t precs[7] = { 1, 2, 24, 53, 64, 128, 200 };
        const char *prefixes[5] = { "x", "y", "DEBUG", "tmp", "" };
        int emitted = 0;
        while (emitted < 50) {
            const uint64_t prec = precs[xs64_below(&rng, 7)];
            const char *prefix = prefixes[xs64_below(&rng, 5)];
            mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
            const uint64_t r = xs64_next(&rng);
            const int64_t e = (int64_t)(r % 401) - 200;
            const double base = (double)(xs64_next(&rng) | 1) / 18446744073709551616.0;
            const int sgn = (xs64_below(&rng, 2)) ? +1 : -1;
            mpfr_set_d(x, sgn * base * ldexp(1.0, (int)e), MPFR_RNDN);
            if (mpfr_regular_p(x)) {
                emit_case(out, "fuzz", prefix, x);
                emitted++;
            }
            mpfr_clear(x);
        }
    }

    /* mined: 5 -- no direct test in mpfr/tests for this debug helper.
     * We use common-use patterns derived from MPFR source debug call sites. */
    emit_d(out, "mined", "x", 1.0, 53);
    emit_d(out, "mined", "x", 0.5, 53);
    emit_d(out, "mined", "x", 2.0, 53);
    emit_d(out, "mined", "y", 3.14, 53);
    emit_d(out, "mined", "tmp", 1.0, 100);

    return 0;
}
