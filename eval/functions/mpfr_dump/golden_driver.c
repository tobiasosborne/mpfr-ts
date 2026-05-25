/*
 * golden_driver.c -- Golden master for MPFR's mpfr_dump.
 *
 * C dispatches to mpfr_fdump(stdout, x). Driver redirects stdout to a
 * tmpfile, calls mpfr_dump, reads output back, emits string scalar.
 * Mirrors the mpfr_fdump driver pattern.
 *
 * Wire: {"inputs":{"x":<mpfr>},"output":"<formatted dump>"}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * Ref: mpfr/src/dump.c L127-L131.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_dump golden_driver requires GMP_NUMB_BITS == 64"
#endif

extern void mpfr_dump(mpfr_srcptr);

static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    /* Redirect stdout to a tmpfile to capture mpfr_dump's output. */
    char buf[4096];
    fflush(stdout);
    int saved_stdout = dup(1);
    FILE *tmp = tmpfile();
    if (!tmp) { fprintf(stderr, "tmpfile failed\n"); exit(2); }
    int tmpfd = fileno(tmp);
    dup2(tmpfd, 1);
    const uint64_t t0 = now_ns();
    mpfr_dump(x);
    const uint64_t elapsed = now_ns() - t0;
    fflush(stdout);
    dup2(saved_stdout, 1);
    close(saved_stdout);
    rewind(tmp);
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[n] = '\0';
    fclose(tmp);

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    jl_output_scalar_str(out, buf);
    jl_finish(out, elapsed);
}

static inline void emit_from_double(FILE *out, const char *tag, double d, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_d(x, d, MPFR_RNDN); emit_case(out, tag, x); mpfr_clear(x);
}
static inline void emit_inf(FILE *out, const char *tag, int sign, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_inf(x, sign); emit_case(out, tag, x); mpfr_clear(x);
}
static inline void emit_zero(FILE *out, const char *tag, int sign, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_zero(x, sign); emit_case(out, tag, x); mpfr_clear(x);
}
static inline void emit_nan(FILE *out, const char *tag, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_nan(x); emit_case(out, tag, x); mpfr_clear(x);
}
static inline void emit_pow2(FILE *out, const char *tag, mpfr_prec_t prec, mpfr_exp_t e) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_ui(x, 1, MPFR_RNDN); mpfr_set_exp(x, e);
    emit_case(out, tag, x); mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- common values across signs and precs. */
    emit_from_double(out, "happy", 0.5, 53);
    emit_from_double(out, "happy", 1.0, 53);
    emit_from_double(out, "happy", 2.0, 53);
    emit_from_double(out, "happy", -1.0, 53);
    emit_from_double(out, "happy", 3.14, 53);
    emit_from_double(out, "happy", -3.14, 53);
    emit_from_double(out, "happy", 100.0, 53);
    emit_from_double(out, "happy", 0.1, 53);
    emit_from_double(out, "happy", 1e10, 53);
    emit_from_double(out, "happy", 1e-10, 53);
    emit_from_double(out, "happy", 1.0, 24);
    emit_from_double(out, "happy", 1.0, 100);
    emit_from_double(out, "happy", 2.5, 53);
    emit_from_double(out, "happy", -2.5, 53);
    emit_inf(out, "happy", +1, 53);
    emit_inf(out, "happy", -1, 53);
    emit_zero(out, "happy", +1, 53);
    emit_zero(out, "happy", -1, 53);
    emit_nan(out, "happy", 53);
    emit_from_double(out, "happy", 7.0, 64);

    /* edge: 30 -- singulars at varied precs, powers of 2, prec extremes. */
    emit_nan(out, "edge", 1);
    emit_nan(out, "edge", 100);
    emit_nan(out, "edge", 256);
    emit_inf(out, "edge", +1, 1);
    emit_inf(out, "edge", -1, 1);
    emit_inf(out, "edge", +1, 100);
    emit_inf(out, "edge", -1, 100);
    emit_zero(out, "edge", +1, 1);
    emit_zero(out, "edge", -1, 1);
    emit_zero(out, "edge", +1, 100);
    emit_zero(out, "edge", -1, 100);
    emit_pow2(out, "edge", 53, 0);
    emit_pow2(out, "edge", 53, 1);
    emit_pow2(out, "edge", 53, -1);
    emit_pow2(out, "edge", 53, 10);
    emit_pow2(out, "edge", 53, -10);
    emit_pow2(out, "edge", 53, 100);
    emit_pow2(out, "edge", 53, -100);
    emit_pow2(out, "edge", 1, 0);
    emit_pow2(out, "edge", 1, 5);
    emit_pow2(out, "edge", 64, 0);
    emit_pow2(out, "edge", 128, 0);
    emit_from_double(out, "edge", 0.5, 1);
    emit_from_double(out, "edge", 1.0, 1);
    emit_from_double(out, "edge", -0.5, 1);
    emit_from_double(out, "edge", 1.0, 2);
    emit_from_double(out, "edge", 3.0, 2);
    emit_from_double(out, "edge", 1.5, 3);
    emit_from_double(out, "edge", 1.25, 4);
    emit_from_double(out, "edge", 1.125, 5);

    /* adversarial: 12 */
    emit_from_double(out, "adversarial", 0.3333333333333333, 53);
    emit_from_double(out, "adversarial", 2.718281828459045, 53);
    emit_from_double(out, "adversarial", 1e308, 53);
    emit_from_double(out, "adversarial", 1e-300, 53);
    emit_pow2(out, "adversarial", 53, 1000);
    emit_pow2(out, "adversarial", 53, -1000);
    emit_pow2(out, "adversarial", 256, 0);
    emit_pow2(out, "adversarial", 1000, 0);
    emit_from_double(out, "adversarial", -0.0001, 53);
    emit_from_double(out, "adversarial", 99999999.99, 64);
    emit_inf(out, "adversarial", +1, 1024);
    emit_zero(out, "adversarial", -1, 1024);

    /* fuzz: 50 -- random doubles at random precs. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xD0D0FACE12345678ULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 128);
            const uint64_t bits = xs64_next(&rng);
            const double d = (double)(int64_t)bits * (1.0 / 1e10);
            mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
            mpfr_set_d(x, d, MPFR_RNDN);
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
        }
    }

    /* mined: 5 -- representative dump format cases. */
    emit_zero(out, "mined", +1, 53);
    emit_zero(out, "mined", -1, 53);
    emit_inf(out, "mined", +1, 53);
    emit_nan(out, "mined", 53);
    emit_from_double(out, "mined", 1.5, 53);

    return 0;
}
