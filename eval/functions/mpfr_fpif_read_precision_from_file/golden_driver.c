/*
 * golden_driver.c -- Golden master for mpfr_fpif_read_precision_from_file.
 *
 * The C function is `static` in mpfr/src/fpif.c (L248-L302). We
 * re-implement the algorithm (golden-driver-substitute pattern per
 * ADR 0002) using portable little-endian byte reading. Both driver
 * and TS port mirror the same substituted algorithm.
 *
 * The driver generates byte buffers by INVERTING mpfr_fpif_store_precision
 * (we re-use that algorithm here) so the read is exercised against
 * well-formed inputs only. Per ADR 0004 Invariant 5, malformed inputs
 * throw on the TS side; we keep them out of the golden because the
 * runner classifies a throw as failure.
 *
 * Wire format (per ADR 0004):
 *   inputs: { "bytes_value": "<decimal-bigint>",
 *             "byte_length": <int>,
 *             "pos": <int> }
 *   output: { "precision": "<decimal>", "nextPos": <int> }
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * Ref: mpfr/src/fpif.c L248-L302 -- C function body.
 * Ref: docs/adr/0004-binary-io-api.md -- API + wire ADR.
 */
#include "common.h"
#include <assert.h>
#include <gmp.h>
#include <inttypes.h>
#include <string.h>

#define MPFR_MAX_PRECSIZE 7
#define MPFR_MAX_EMBEDDED_PRECISION (255 - MPFR_MAX_PRECSIZE)

/* Encode precision -> bytes (inverse fixture). */
static size_t encode_precision(unsigned char *out, uint64_t precision) {
    assert(precision >= 1);
    if (precision <= MPFR_MAX_EMBEDDED_PRECISION) {
        out[0] = (unsigned char)(precision + MPFR_MAX_PRECSIZE);
        return 1;
    }
    uint64_t copy = precision - (MPFR_MAX_EMBEDDED_PRECISION + 1);
    size_t size_precision = 0;
    uint64_t tmp = copy;
    do {
        tmp >>= 8;
        size_precision++;
    } while (tmp != 0);
    out[0] = (unsigned char)(size_precision - 1);
    for (size_t i = 0; i < size_precision; ++i) {
        out[1 + i] = (unsigned char)((copy >> (8 * i)) & 0xff);
    }
    return 1 + size_precision;
}

/* Decode: mirrors fpif.c L248-L302 with the host-endian abstraction
 * collapsed to LE. Returns precision; writes nextPos to *next_pos.
 * Aborts on malformed input (the driver only feeds well-formed bytes). */
static uint64_t decode_precision(const unsigned char *bytes, size_t buf_len,
                                  size_t pos, size_t *next_pos) {
    assert(pos < buf_len);
    unsigned char first = bytes[pos];
    pos++;
    if (first > MPFR_MAX_PRECSIZE) {
        *next_pos = pos;
        return (uint64_t)(first - MPFR_MAX_PRECSIZE);
    }
    size_t precision_size = (size_t)first + 1;
    /* Must fit in 8 bytes (mpfr_uprec_t = uint64_t). */
    assert(precision_size <= 8);
    assert(pos + precision_size <= buf_len);
    uint64_t copy = 0;
    for (size_t i = 0; i < precision_size; ++i) {
        copy |= (uint64_t)bytes[pos + i] << (8 * i);
    }
    /* Bound check: top bit of last byte set means overflow into signed prec_t. */
    if (precision_size == 8 && (bytes[pos + 7] & 0x80)) {
        /* Driver should not generate this -- fail loud. */
        fprintf(stderr, "decode_precision: top-bit overflow\n");
        exit(2);
    }
    pos += precision_size;
    *next_pos = pos;
    return copy + (MPFR_MAX_EMBEDDED_PRECISION + 1);
}

static void bytes_to_mpz(mpz_t z, const unsigned char *buf, size_t n) {
    mpz_set_ui(z, 0);
    for (size_t i = n; i > 0; --i) {
        mpz_mul_2exp(z, z, 8);
        mpz_add_ui(z, z, buf[i - 1]);
    }
}

/* Emit a case: encode `precision` into bytes (optionally prefixed by
 * `prefix_len` zero filler / position offset), then decode and emit. */
static void emit_case(FILE *out, const char *tag,
                      uint64_t precision, size_t prefix_len) {
    unsigned char buf[64] = {0};
    /* prefix_len leading bytes of any value -- here zeros, but
     * extensible. They are skipped over by `pos = prefix_len`. */
    size_t n_enc = encode_precision(buf + prefix_len, precision);
    size_t buf_len = prefix_len + n_enc;
    size_t pos = prefix_len;
    size_t next_pos = 0;

    const uint64_t t0 = now_ns();
    uint64_t got = decode_precision(buf, buf_len, pos, &next_pos);
    const uint64_t elapsed = now_ns() - t0;
    assert(got == precision);
    assert(next_pos == buf_len);

    /* Inputs. */
    mpz_t z;
    mpz_init(z);
    bytes_to_mpz(z, buf, buf_len);
    char *bytes_str = mpz_get_str(NULL, 10, z);

    jl_begin(out, tag);
    fprintf(out, "\"bytes_value\":\"%s\"", bytes_str);
    fprintf(out, ",\"byte_length\":%zu", buf_len);
    fprintf(out, ",\"pos\":%zu", pos);
    jl_end_inputs(out);

    fprintf(out, ",\"output\":{\"precision\":\"%" PRIu64 "\","
                 "\"nextPos\":%zu}", got, next_pos);
    jl_finish(out, elapsed);

    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(bytes_str, strlen(bytes_str) + 1);
    mpz_clear(z);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- common precisions, pos=0. */
    {
        const uint64_t precs[20] = {
            1, 2, 24, 53, 64, 100, 113, 128, 130, 200,
            240, 248, 249, 256, 512, 1000, 1024, 2000, 2048, 4096,
        };
        for (int i = 0; i < 20; ++i) emit_case(out, "happy", precs[i], 0);
    }

    /* edge: 30 -- boundaries + nonzero pos. */
    emit_case(out, "edge", 1, 0);
    emit_case(out, "edge", 7, 0);
    emit_case(out, "edge", 8, 0);
    emit_case(out, "edge", 248, 0);            /* MPFR_MAX_EMBEDDED_PRECISION */
    emit_case(out, "edge", 249, 0);            /* first extended */
    emit_case(out, "edge", 250, 0);
    emit_case(out, "edge", 504, 0);            /* p-249=255, 1 byte */
    emit_case(out, "edge", 505, 0);            /* p-249=256, 2 bytes */
    emit_case(out, "edge", 65784, 0);          /* p-249=65535, 2 bytes */
    emit_case(out, "edge", 65785, 0);          /* p-249=65536, 3 bytes */
    emit_case(out, "edge", 16777464, 0);       /* 3-byte boundary */
    emit_case(out, "edge", 16777465, 0);       /* 4-byte boundary */
    emit_case(out, "edge", 4294967544ULL, 0);  /* 4-byte boundary */
    emit_case(out, "edge", 4294967545ULL, 0);  /* 5-byte boundary */
    /* pos > 0 -- exercise the cursor. */
    emit_case(out, "edge", 53, 1);
    emit_case(out, "edge", 53, 3);
    emit_case(out, "edge", 53, 7);
    emit_case(out, "edge", 53, 16);
    emit_case(out, "edge", 130, 5);
    emit_case(out, "edge", 2048, 10);
    emit_case(out, "edge", 1024, 16);
    emit_case(out, "edge", 249, 2);
    emit_case(out, "edge", 65784, 4);
    emit_case(out, "edge", 1099511627776ULL, 0); /* p-249 ~ 2^40 (6 bytes) */
    emit_case(out, "edge", 281474976710656ULL, 0); /* 7 bytes */
    emit_case(out, "edge", 100, 1);
    emit_case(out, "edge", 200, 1);
    emit_case(out, "edge", 1, 1);
    emit_case(out, "edge", 248, 8);
    emit_case(out, "edge", 9, 0);

    /* adversarial: 12 -- large precs, mix of pos. */
    emit_case(out, "adversarial", 1099511627776ULL, 7);
    emit_case(out, "adversarial", 281474976710656ULL, 5);
    emit_case(out, "adversarial", 72057594037927936ULL, 0);
    /* p such that p-249 hits UINT64_MAX/2 (still safe under top-bit check) */
    emit_case(out, "adversarial", 9223372036854775807ULL - 248ULL, 0);
    emit_case(out, "adversarial", 549755813632ULL, 3);
    emit_case(out, "adversarial", 140737488355328ULL, 2);
    emit_case(out, "adversarial", 36028797018963968ULL, 1);
    emit_case(out, "adversarial", 4294967296ULL, 4);
    emit_case(out, "adversarial", 4294967295ULL, 4);
    emit_case(out, "adversarial", 250, 12);
    emit_case(out, "adversarial", 251, 13);
    emit_case(out, "adversarial", 252, 14);

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xDEADC0DECAFEFEEDULL);
        for (int rep = 0; rep < 50; ++rep) {
            uint64_t mode = xs64_below(&rng, 4);
            uint64_t precision;
            if (mode == 0) {
                precision = 1 + xs64_below(&rng, MPFR_MAX_EMBEDDED_PRECISION);
            } else if (mode == 1) {
                precision = (MPFR_MAX_EMBEDDED_PRECISION + 1) + xs64_below(&rng, 65536);
            } else if (mode == 2) {
                precision = (MPFR_MAX_EMBEDDED_PRECISION + 1) + xs64_below(&rng, 1ULL << 32);
            } else {
                uint64_t r = xs64_next(&rng) >> 1;  /* keep top bit clear */
                precision = (MPFR_MAX_EMBEDDED_PRECISION + 1) + r;
            }
            size_t prefix = (size_t)xs64_below(&rng, 16);
            emit_case(out, "fuzz", precision, prefix);
        }
    }

    /* mined: 5 -- precisions exercised by mpfr/tests/tfpif.c. */
    emit_case(out, "mined", 130, 0);   /* tfpif.c p1 */
    emit_case(out, "mined", 2048, 0);  /* tfpif.c p2 */
    emit_case(out, "mined", 1, 0);
    emit_case(out, "mined", 53, 0);
    emit_case(out, "mined", 17, 0);

    return 0;
}
