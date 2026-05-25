/*
 * golden_driver.c -- Golden master for mpfr_fpif_store_exponent.
 *
 * The C function is `static` in mpfr/src/fpif.c (L317-L387). We
 * re-implement the algorithm (golden-driver-substitute pattern per
 * ADR 0002) using portable little-endian byte manipulation. Both the
 * driver and the TS port mirror the same substituted algorithm.
 *
 * Wire format (per ADR 0004):
 *   inputs: { "x": <MPFR wire> }
 *   output: { "bytes": "<decimal-bigint>", "byte_length": <int> }
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * Ref: mpfr/src/fpif.c L44-L57 -- exponent field format spec.
 * Ref: mpfr/src/fpif.c L317-L387 -- C function body.
 * Ref: docs/adr/0004-binary-io-api.md -- API + wire ADR.
 */
#include "common.h"
#include <assert.h>
#include <gmp.h>
#include <inttypes.h>
#include <string.h>

#define MPFR_KIND_ZERO 119
#define MPFR_KIND_INF 120
#define MPFR_KIND_NAN 121
#define MPFR_MAX_EMBEDDED_EXPONENT 47
#define MPFR_EXTERNAL_EXPONENT 94

/* Emit the exponent byte buffer for an MPFR value. Mirrors fpif.c
 * L317-L387 verbatim with the host-endian path collapsed to LE. The
 * out buffer must be at least 17 bytes (1 header + up to 16 payload). */
static size_t encode_exponent(unsigned char *out, mpfr_srcptr x) {
    size_t exponent_size = 0;
    uint64_t uexp = 0;

    if (mpfr_regular_p(x)) {
        mpfr_exp_t exponent = mpfr_get_exp(x);
        if (exponent > MPFR_MAX_EMBEDDED_EXPONENT ||
            exponent < -MPFR_MAX_EMBEDDED_EXPONENT) {
            /* |e| - 47 fits in mpfr_uexp_t since e is in mpfr_exp_t range. */
            uint64_t abs_e = exponent < 0 ? (uint64_t)(-(int64_t)exponent)
                                          : (uint64_t)exponent;
            uexp = abs_e - MPFR_MAX_EMBEDDED_EXPONENT;
            uint64_t copy_exponent = uexp << 1; /* room for sign bit */
            assert(copy_exponent > uexp);       /* C MPFR_ASSERTD: no overflow */
            uint64_t tmp = copy_exponent;
            do {
                tmp >>= 8;
                exponent_size++;
            } while (tmp != 0);
            assert(exponent_size <= 16);
            uint64_t exp_sign_bit = (uint64_t)1 << (8 * exponent_size - 1);
            assert(uexp < exp_sign_bit);
            if (exponent < 0) uexp |= exp_sign_bit;
        } else {
            /* embedded */
            uexp = (uint64_t)(exponent + MPFR_MAX_EMBEDDED_EXPONENT);
        }
    }

    /* Now write the header + payload. */
    if (mpfr_regular_p(x)) {
        if (exponent_size == 0) {
            out[0] = (unsigned char)uexp;
        } else {
            out[0] = (unsigned char)(MPFR_EXTERNAL_EXPONENT + exponent_size);
            for (size_t i = 0; i < exponent_size; ++i) {
                out[1 + i] = (unsigned char)((uexp >> (8 * i)) & 0xff);
            }
        }
    } else if (mpfr_zero_p(x)) {
        out[0] = MPFR_KIND_ZERO;
    } else if (mpfr_inf_p(x)) {
        out[0] = MPFR_KIND_INF;
    } else {
        assert(mpfr_nan_p(x));
        out[0] = MPFR_KIND_NAN;
    }

    if (MPFR_SIGN(x) < 0) out[0] |= 0x80;
    return 1 + exponent_size;
}

static void bytes_to_mpz(mpz_t z, const unsigned char *buf, size_t n) {
    mpz_set_ui(z, 0);
    for (size_t i = n; i > 0; --i) {
        mpz_mul_2exp(z, z, 8);
        mpz_add_ui(z, z, buf[i - 1]);
    }
}

static void emit_output(FILE *out, const unsigned char *buf, size_t n) {
    mpz_t z;
    mpz_init(z);
    bytes_to_mpz(z, buf, n);
    char *s = mpz_get_str(NULL, 10, z);
    fprintf(out, ",\"output\":{\"bytes\":\"%s\",\"byte_length\":%zu}", s, n);
    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(s, strlen(s) + 1);
    mpz_clear(z);
}

static void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    unsigned char buf[32] = {0};
    const uint64_t t0 = now_ns();
    size_t n = encode_exponent(buf, x);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    emit_output(out, buf, n);
    jl_finish(out, elapsed);
}

/* Build an MPFR value at given precision and exponent (mantissa = 1.0
 * shifted to the requested exponent). */
static void set_normal_with_exp(mpfr_t x, mpfr_prec_t prec, mpfr_exp_t e, int sign) {
    mpfr_set_prec(x, prec);
    mpfr_set_ui(x, 1, MPFR_RNDN);  /* 1.0 with given prec */
    mpfr_set_exp(x, e);
    if (sign < 0) mpfr_neg(x, x, MPFR_RNDN);
}

/* Helper: set x to NaN with positive sign (TS NaN canonical). */
static void set_positive_nan(mpfr_t x) {
    mpfr_set_zero(x, 1);  /* +0 first to fix the sign bit */
    mpfr_set_nan(x);      /* preserves sign */
}

int main(void) {
    FILE *out = stdout;
    mpfr_t x;
    mpfr_init2(x, 53);

    /* happy: 20 -- typical exponents in the embedded range. */
    for (int i = 0; i < 20; ++i) {
        mpfr_exp_t e = -10 + i;  /* -10..9, all embedded */
        set_normal_with_exp(x, 53, e, 1);
        emit_case(out, "happy", x);
    }

    /* edge: 30 -- boundary transitions in the format. */
    /* Embedded boundary cases. */
    set_normal_with_exp(x, 53, -47, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53,  47, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53, -47, -1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53,  47, -1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53,  0, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53,  0, -1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53, -48, 1); emit_case(out, "edge", x);  /* first extended */
    set_normal_with_exp(x, 53,  48, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53, -48, -1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53,  48, -1); emit_case(out, "edge", x);
    /* Boundary at uexp << 1 spans 1 vs 2 bytes. uexp = |e|-47. */
    set_normal_with_exp(x, 53, 47+127, 1); emit_case(out, "edge", x);  /* uexp=127, << 1 = 254, 1 byte */
    set_normal_with_exp(x, 53, 47+128, 1); emit_case(out, "edge", x);  /* uexp=128, << 1 = 256, 2 bytes */
    set_normal_with_exp(x, 53, -(47+127), 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53, -(47+128), 1); emit_case(out, "edge", x);
    /* Singular values, both signs. */
    mpfr_set_zero(x, 1);  emit_case(out, "edge", x);   /* +0 */
    mpfr_set_zero(x, -1); emit_case(out, "edge", x);   /* -0 */
    mpfr_set_inf(x, 1);   emit_case(out, "edge", x);   /* +inf */
    mpfr_set_inf(x, -1);  emit_case(out, "edge", x);   /* -inf */
    set_positive_nan(x);  emit_case(out, "edge", x);   /* NaN (sign=+ per TS convention) */
    /* Larger embedded range probes. */
    set_normal_with_exp(x, 53, -1, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53,  1, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53, -46, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53,  46, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53, -49, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53,  49, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53,  100, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53, -100, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53,  1000, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53, -1000, 1); emit_case(out, "edge", x);
    set_normal_with_exp(x, 53,  47+255, 1); emit_case(out, "edge", x);

    /* adversarial: 12 -- large extended exponents. */
    set_normal_with_exp(x, 53,  47 + (1 << 15), 1); emit_case(out, "adversarial", x);
    set_normal_with_exp(x, 53, -(47 + (1 << 15)), 1); emit_case(out, "adversarial", x);
    set_normal_with_exp(x, 53,  47 + (1 << 20), 1); emit_case(out, "adversarial", x);
    set_normal_with_exp(x, 53, -(47 + (1 << 20)), 1); emit_case(out, "adversarial", x);
    set_normal_with_exp(x, 53,  47 + ((mpfr_exp_t)1 << 28), 1); emit_case(out, "adversarial", x);
    set_normal_with_exp(x, 53, -(47 + ((mpfr_exp_t)1 << 28)), 1); emit_case(out, "adversarial", x);
    set_normal_with_exp(x, 53,  47 + 32767, 1); emit_case(out, "adversarial", x);  /* 2-byte boundary */
    set_normal_with_exp(x, 53,  47 + 32768, 1); emit_case(out, "adversarial", x);  /* 3-byte */
    set_normal_with_exp(x, 53,  47 + 8388608, 1); emit_case(out, "adversarial", x); /* 4-byte */
    /* Sign-bit interplay (negative MPFR with large positive exponent). */
    set_normal_with_exp(x, 53, 1000, -1); emit_case(out, "adversarial", x);
    set_normal_with_exp(x, 53, -1000, -1); emit_case(out, "adversarial", x);
    set_normal_with_exp(x, 53, 100000, -1); emit_case(out, "adversarial", x);

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xBADC0FFEE0DDF00DULL);
        for (int rep = 0; rep < 50; ++rep) {
            uint64_t mode = xs64_below(&rng, 8);
            if (mode == 0) {
                mpfr_set_zero(x, (xs64_below(&rng, 2)) ? -1 : 1);
            } else if (mode == 1) {
                mpfr_set_inf(x, (xs64_below(&rng, 2)) ? -1 : 1);
            } else if (mode == 2) {
                /* TS NaN convention forces sign=+1. */
                set_positive_nan(x);
            } else {
                /* Random exponent in [-2^28, 2^28], sign random. */
                int64_t raw = (int64_t)(xs64_next(&rng) & 0x1FFFFFFFULL);
                if (xs64_below(&rng, 2)) raw = -raw;
                mpfr_exp_t e = (mpfr_exp_t)raw;
                int sign = (xs64_below(&rng, 2)) ? -1 : 1;
                set_normal_with_exp(x, 53, e, sign);
            }
            emit_case(out, "fuzz", x);
        }
    }

    /* mined: 5 -- exponents exercised by mpfr/tests/tfpif.c. */
    /* tfpif.c L52: mpfr_set_str1(x[0], "45.2564...") -- value ~45, exp ~6. */
    set_normal_with_exp(x, 130, 6, 1); emit_case(out, "mined", x);
    /* tfpif.c L54: mpfr_set_exp(x[2], -48000). */
    set_normal_with_exp(x, 2048, -48000, 1); emit_case(out, "mined", x);
    /* tfpif.c L55-58: inf, zero, nan. */
    mpfr_set_inf(x, 1);  emit_case(out, "mined", x);
    mpfr_set_zero(x, 1); emit_case(out, "mined", x);
    set_positive_nan(x); emit_case(out, "mined", x);

    mpfr_clear(x);
    return 0;
}
