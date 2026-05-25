/*
 * golden_driver.c -- Golden master for mpfr_fpif_read_exponent_from_file.
 *
 * The C function is `static` in mpfr/src/fpif.c (L399-L473). We
 * re-implement the algorithm (golden-driver-substitute pattern per
 * ADR 0002) using portable LE byte reading. Driver and TS port mirror
 * the same substituted algorithm.
 *
 * The driver generates byte buffers by INVERTING mpfr_fpif_store_exponent
 * (re-used inline). Only well-formed inputs appear in the golden.
 *
 * Wire format (per ADR 0004):
 *   inputs: { "bytes_value": "<decimal-bigint>",
 *             "byte_length": <int>,
 *             "pos": <int>,
 *             "prec": "<decimal>" }
 *   output: { "kind": "<string>",
 *             "sign": <int 1|-1>,
 *             "exp": "<decimal>",
 *             "nextPos": <int> }
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * Ref: mpfr/src/fpif.c L399-L473 -- C function body.
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

/* Bounds on mpfr_exp_t / valid exponent range. We track the default
 * emin/emax (per src/core.ts and the assertions in mpfr/src/init2.c
 * MPFR_EMIN_DEFAULT/EMAX_DEFAULT = +/-(2^30 - 1)). The driver only
 * generates exponents within these bounds. */
#define DEFAULT_EMIN_MIN (-((mpfr_exp_t)1 << 30) + 1)
#define DEFAULT_EMAX_MAX ((mpfr_exp_t)1 << 30) - 1

/* Encode (kind, sign, exp) -> bytes. Returns length. */
static size_t encode_exponent(unsigned char *out, int kind_enum,
                              int sign, int64_t exponent) {
    size_t exponent_size = 0;
    uint64_t uexp = 0;
    unsigned char header;

    if (kind_enum == 0) {
        /* normal */
        if (exponent > MPFR_MAX_EMBEDDED_EXPONENT ||
            exponent < -MPFR_MAX_EMBEDDED_EXPONENT) {
            uint64_t abs_e = exponent < 0 ? (uint64_t)(-exponent)
                                          : (uint64_t)exponent;
            uexp = abs_e - MPFR_MAX_EMBEDDED_EXPONENT;
            uint64_t copy_exponent = uexp << 1;
            uint64_t tmp = copy_exponent;
            do {
                tmp >>= 8;
                exponent_size++;
            } while (tmp != 0);
            uint64_t exp_sign_bit = (uint64_t)1 << (8 * exponent_size - 1);
            if (exponent < 0) uexp |= exp_sign_bit;
            header = (unsigned char)(MPFR_EXTERNAL_EXPONENT + exponent_size);
        } else {
            uexp = (uint64_t)(exponent + MPFR_MAX_EMBEDDED_EXPONENT);
            header = (unsigned char)uexp;
        }
    } else if (kind_enum == 1) {
        header = MPFR_KIND_ZERO;
    } else if (kind_enum == 2) {
        header = MPFR_KIND_INF;
    } else {
        /* kind_enum == 3: nan */
        header = MPFR_KIND_NAN;
    }
    if (sign < 0) header |= 0x80;
    out[0] = header;
    if (kind_enum == 0 && exponent_size > 0) {
        for (size_t i = 0; i < exponent_size; ++i) {
            out[1 + i] = (unsigned char)((uexp >> (8 * i)) & 0xff);
        }
    }
    return 1 + exponent_size;
}

/* Decode result. */
typedef struct {
    int kind_enum;      /* 0 normal, 1 zero, 2 inf, 3 nan */
    int sign;
    int64_t exponent;   /* meaningful only for kind_enum == 0 */
    size_t next_pos;
} decoded_t;

/* Decode (mirror fpif.c L399-L473). */
static decoded_t decode_exponent(const unsigned char *bytes, size_t buf_len,
                                  size_t pos) {
    decoded_t r = {0, 1, 0, 0};
    assert(pos < buf_len);
    unsigned char b0 = bytes[pos];
    pos++;
    r.sign = (b0 & 0x80) ? -1 : 1;
    unsigned char e = b0 & 0x7F;
    size_t exponent_size = 1;

    if (e > MPFR_EXTERNAL_EXPONENT && e < MPFR_KIND_ZERO) {
        exponent_size = e - MPFR_EXTERNAL_EXPONENT;
        assert(exponent_size <= 8);   /* mpfr_exp_t fits */
        assert(pos + exponent_size <= buf_len);

        uint64_t uexp = 0;
        for (size_t i = 0; i < exponent_size; ++i) {
            uexp |= (uint64_t)bytes[pos + i] << (8 * i);
        }
        uint64_t exp_sign_bit = uexp & ((uint64_t)1 << (8 * exponent_size - 1));
        uexp &= ~exp_sign_bit;
        uexp += MPFR_MAX_EMBEDDED_EXPONENT;
        int64_t exponent = exp_sign_bit ? -(int64_t)uexp : (int64_t)uexp;
        r.kind_enum = 0;
        r.exponent = exponent;
        pos += exponent_size;
        exponent_size++;  /* matches C: exponent_size++ accounts for header */
    } else if (e == MPFR_KIND_ZERO) {
        r.kind_enum = 1;
    } else if (e == MPFR_KIND_INF) {
        r.kind_enum = 2;
    } else if (e == MPFR_KIND_NAN) {
        r.kind_enum = 3;
    } else if (e <= MPFR_EXTERNAL_EXPONENT) {
        /* embedded */
        r.kind_enum = 0;
        r.exponent = (int64_t)e - MPFR_MAX_EMBEDDED_EXPONENT;
    } else {
        fprintf(stderr, "decode_exponent: invalid byte 0x%02x\n", e);
        exit(2);
    }
    r.next_pos = pos;
    return r;
}

static const char *kind_str(int kind_enum) {
    switch (kind_enum) {
        case 0: return "normal";
        case 1: return "zero";
        case 2: return "inf";
        case 3: return "nan";
        default: return "?";
    }
}

static void bytes_to_mpz(mpz_t z, const unsigned char *buf, size_t n) {
    mpz_set_ui(z, 0);
    for (size_t i = n; i > 0; --i) {
        mpz_mul_2exp(z, z, 8);
        mpz_add_ui(z, z, buf[i - 1]);
    }
}

static void emit_case(FILE *out, const char *tag,
                      int kind_enum, int sign, int64_t exponent,
                      uint64_t prec, size_t prefix_len) {
    unsigned char buf[64] = {0};
    size_t enc_len = encode_exponent(buf + prefix_len, kind_enum, sign, exponent);
    size_t buf_len = prefix_len + enc_len;
    size_t pos = prefix_len;

    const uint64_t t0 = now_ns();
    decoded_t r = decode_exponent(buf, buf_len, pos);
    const uint64_t elapsed = now_ns() - t0;
    /* Sanity: round-trip identity. */
    assert(r.kind_enum == kind_enum);
    assert(r.sign == sign);
    if (kind_enum == 0) assert(r.exponent == exponent);
    assert(r.next_pos == buf_len);

    mpz_t z;
    mpz_init(z);
    bytes_to_mpz(z, buf, buf_len);
    char *bytes_str = mpz_get_str(NULL, 10, z);

    jl_begin(out, tag);
    fprintf(out, "\"bytes_value\":\"%s\"", bytes_str);
    fprintf(out, ",\"byte_length\":%zu", buf_len);
    fprintf(out, ",\"pos\":%zu", pos);
    fprintf(out, ",\"prec\":\"%" PRIu64 "\"", prec);
    jl_end_inputs(out);

    /* For singular kinds, exp = 0 (per src/core.ts convention; the
     * substrate returns the actual exp encoded which is 0 for singular). */
    int64_t out_exp = (kind_enum == 0) ? r.exponent : 0;
    fprintf(out, ",\"output\":{\"kind\":\"%s\",\"sign\":%d,"
                 "\"exp\":\"%" PRId64 "\",\"nextPos\":%zu}",
            kind_str(r.kind_enum), r.sign, out_exp, r.next_pos);
    jl_finish(out, elapsed);

    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(bytes_str, strlen(bytes_str) + 1);
    mpz_clear(z);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- typical exponents, pos=0, both signs. */
    int64_t happy_exps[10] = {0, 1, -1, 5, -5, 10, -10, 30, -30, 45};
    for (int i = 0; i < 10; ++i) {
        emit_case(out, "happy", 0, 1, happy_exps[i], 53, 0);
    }
    for (int i = 0; i < 10; ++i) {
        emit_case(out, "happy", 0, -1, happy_exps[i], 53, 0);
    }

    /* edge: 30 -- boundary cases. */
    /* Embedded boundary exponents. */
    emit_case(out, "edge", 0, 1, -47, 53, 0);
    emit_case(out, "edge", 0, 1,  47, 53, 0);
    emit_case(out, "edge", 0, -1, -47, 53, 0);
    emit_case(out, "edge", 0, -1,  47, 53, 0);
    emit_case(out, "edge", 0, 1, -48, 53, 0);  /* first extended */
    emit_case(out, "edge", 0, 1,  48, 53, 0);
    emit_case(out, "edge", 0, -1, -48, 53, 0);
    emit_case(out, "edge", 0, -1,  48, 53, 0);
    /* Byte-boundary transitions for extended exponents. */
    emit_case(out, "edge", 0, 1, 47 + 127, 53, 0);   /* uexp<<1=254, 1 byte */
    emit_case(out, "edge", 0, 1, 47 + 128, 53, 0);   /* uexp<<1=256, 2 bytes */
    emit_case(out, "edge", 0, 1, -(47 + 127), 53, 0);
    emit_case(out, "edge", 0, 1, -(47 + 128), 53, 0);
    /* Singular kinds. */
    emit_case(out, "edge", 1, 1, 0, 53, 0);   /* +0 */
    emit_case(out, "edge", 1, -1, 0, 53, 0);  /* -0 */
    emit_case(out, "edge", 2, 1, 0, 53, 0);   /* +inf */
    emit_case(out, "edge", 2, -1, 0, 53, 0);  /* -inf */
    emit_case(out, "edge", 3, 1, 0, 53, 0);   /* NaN (sign=+) */
    /* pos > 0 -- exercise the cursor. */
    emit_case(out, "edge", 0, 1, 0, 53, 1);
    emit_case(out, "edge", 0, 1, 100, 53, 3);
    emit_case(out, "edge", 0, -1, -100, 53, 5);
    emit_case(out, "edge", 1, 1, 0, 53, 2);
    emit_case(out, "edge", 2, -1, 0, 53, 4);
    /* Various precisions (just a tag, doesn't affect encoding). */
    emit_case(out, "edge", 0, 1, 0, 1, 0);
    emit_case(out, "edge", 0, 1, 0, 2, 0);
    emit_case(out, "edge", 0, 1, 0, 130, 0);
    emit_case(out, "edge", 0, 1, 0, 2048, 0);
    emit_case(out, "edge", 0, 1, 0, 100000, 0);
    /* exponent_size = 2 path. */
    emit_case(out, "edge", 0, 1, 47 + 1000, 53, 0);
    emit_case(out, "edge", 0, -1, -(47 + 1000), 53, 0);
    /* exponent_size = 3. */
    emit_case(out, "edge", 0, 1, 47 + 100000, 53, 0);

    /* adversarial: 12 -- larger exponents, mix of pos. */
    emit_case(out, "adversarial", 0, 1, (int64_t)(1 << 25), 53, 0);
    emit_case(out, "adversarial", 0, -1, -((int64_t)1 << 25), 53, 0);
    emit_case(out, "adversarial", 0, 1, DEFAULT_EMAX_MAX, 53, 0);
    emit_case(out, "adversarial", 0, -1, DEFAULT_EMIN_MIN, 53, 0);
    emit_case(out, "adversarial", 0, 1, 47 + 32767, 53, 4);
    emit_case(out, "adversarial", 0, 1, 47 + 32768, 53, 4);
    emit_case(out, "adversarial", 0, 1, 47 + (1 << 20), 53, 8);
    emit_case(out, "adversarial", 0, -1, -(47 + (1 << 20)), 53, 8);
    emit_case(out, "adversarial", 3, -1, 0, 53, 0);  /* NaN with sign=-1 */
    emit_case(out, "adversarial", 1, -1, 0, 1, 7);   /* -0 with low prec */
    emit_case(out, "adversarial", 2, 1, 0, 4096, 0); /* +inf with high prec */
    emit_case(out, "adversarial", 0, 1, 0, 9223372036854775807LL >> 30, 0); /* large prec */

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xF00DBABECAFED00DULL);
        for (int rep = 0; rep < 50; ++rep) {
            uint64_t mode = xs64_below(&rng, 8);
            int kind_enum;
            int sign = (xs64_below(&rng, 2)) ? -1 : 1;
            int64_t exponent = 0;
            if (mode == 0) kind_enum = 1;
            else if (mode == 1) kind_enum = 2;
            else if (mode == 2) kind_enum = 3;
            else {
                kind_enum = 0;
                /* exponent in [-(1 << 28), 1 << 28) */
                int64_t raw = (int64_t)(xs64_next(&rng) & 0x1FFFFFFFULL);
                if (xs64_below(&rng, 2)) raw = -raw;
                exponent = raw;
            }
            uint64_t prec = 1 + xs64_below(&rng, 4096);
            size_t prefix = (size_t)xs64_below(&rng, 16);
            emit_case(out, "fuzz", kind_enum, sign, exponent, prec, prefix);
        }
    }

    /* mined: 5 -- exponents from mpfr/tests/tfpif.c. */
    /* set_str1("45.2564..."): value ~= 45.25, exponent in mpfr_exp_t. */
    emit_case(out, "mined", 0, 1, 6, 130, 0);
    /* mpfr_set_exp(x[2], -48000). */
    emit_case(out, "mined", 0, 1, -48000, 2048, 0);
    /* mpfr_set_inf(x[3], 1) -> +inf. */
    emit_case(out, "mined", 2, 1, 0, 2048, 0);
    /* mpfr_set_zero(x[4], 1) -> +0. */
    emit_case(out, "mined", 1, 1, 0, 2048, 0);
    /* MPFR_SET_POS + mpfr_set_nan -> NaN with sign=+. */
    emit_case(out, "mined", 3, 1, 0, 2048, 0);

    return 0;
}
