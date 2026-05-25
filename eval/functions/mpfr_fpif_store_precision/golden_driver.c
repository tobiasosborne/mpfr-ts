/*
 * golden_driver.c -- Golden master for mpfr_fpif_store_precision.
 *
 * The C function is `static` in mpfr/src/fpif.c (L208-L240). We
 * re-implement the algorithm (golden-driver-substitute pattern per
 * ADR 0002) using portable little-endian byte manipulation. Both the
 * driver and the TS port mirror the same substituted algorithm, so
 * strict-equality grading is sound.
 *
 * Wire format (per ADR 0004):
 *   inputs: { "precision": "<decimal>" }
 *   output: { "bytes": "<decimal-bigint>", "byte_length": <int> }
 *
 * The bytes-as-bigint is the little-endian unsigned integer interpretation
 * of the produced byte buffer; byte_length is the exact buffer length.
 * Together they pin the buffer contents uniquely (a leading-zero byte
 * cannot be confused with a missing trailing byte).
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * Ref: mpfr/src/fpif.c L30-L43 -- precision field format spec.
 * Ref: mpfr/src/fpif.c L208-L240 -- C function body.
 * Ref: docs/adr/0004-binary-io-api.md -- API + wire ADR.
 */
#include "common.h"
#include <assert.h>
#include <gmp.h>
#include <inttypes.h>
#include <string.h>

#define MPFR_MAX_PRECSIZE 7
#define MPFR_MAX_EMBEDDED_PRECISION (255 - MPFR_MAX_PRECSIZE)

/* Compute the fpif-encoded precision buffer. Mirrors fpif.c L208-L240
 * with the host-endian abstraction collapsed to little-endian (Invariant
 * 3 in ADR 0004). Returns the buffer length; the buffer must be at
 * least 9 bytes (1 header + up to 8 payload). */
static size_t encode_precision(unsigned char *out, uint64_t precision) {
    assert(precision >= 1);
    if (precision <= MPFR_MAX_EMBEDDED_PRECISION) {
        out[0] = (unsigned char)(precision + MPFR_MAX_PRECSIZE);
        return 1;
    }
    uint64_t copy = precision - (MPFR_MAX_EMBEDDED_PRECISION + 1);
    /* COUNT_NB_BYTE: shift right 8, increment size, until copy == 0. */
    size_t size_precision = 0;
    uint64_t tmp = copy;
    do {
        tmp >>= 8;
        size_precision++;
    } while (tmp != 0);
    out[0] = (unsigned char)(size_precision - 1);
    /* Little-endian write of `copy` across size_precision bytes. */
    for (size_t i = 0; i < size_precision; ++i) {
        out[1 + i] = (unsigned char)((copy >> (8 * i)) & 0xff);
    }
    return 1 + size_precision;
}

/* Convert byte buffer (little-endian) to a GMP mpz_t for emit. */
static void bytes_to_mpz(mpz_t z, const unsigned char *buf, size_t n) {
    mpz_set_ui(z, 0);
    for (size_t i = n; i > 0; --i) {
        mpz_mul_2exp(z, z, 8);
        mpz_add_ui(z, z, buf[i - 1]);
    }
}

/* Emit ,"output":{"bytes":"<dec>","byte_length":<n>}. */
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

static void emit_case(FILE *out, const char *tag, uint64_t precision) {
    unsigned char buf[16] = {0};
    const uint64_t t0 = now_ns();
    size_t n = encode_precision(buf, precision);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "precision", precision);
    jl_end_inputs(out);
    emit_output(out, buf, n);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- common precisions in the embedded range. */
    emit_case(out, "happy", 1);
    emit_case(out, "happy", 2);
    emit_case(out, "happy", 24);
    emit_case(out, "happy", 53);
    emit_case(out, "happy", 64);
    emit_case(out, "happy", 100);
    emit_case(out, "happy", 113);
    emit_case(out, "happy", 128);
    emit_case(out, "happy", 130);
    emit_case(out, "happy", 200);
    emit_case(out, "happy", 240);
    emit_case(out, "happy", 248);
    emit_case(out, "happy", 249);
    emit_case(out, "happy", 256);
    emit_case(out, "happy", 512);
    emit_case(out, "happy", 1000);
    emit_case(out, "happy", 1024);
    emit_case(out, "happy", 2000);
    emit_case(out, "happy", 2048);
    emit_case(out, "happy", 4096);

    /* edge: 30 -- boundary transitions in the format. */
    emit_case(out, "edge", 1);                  /* minimum precision */
    emit_case(out, "edge", 7);                  /* tiny */
    emit_case(out, "edge", 8);                  /* B=15, still embedded */
    emit_case(out, "edge", 247);                /* one below embedded boundary */
    emit_case(out, "edge", 248);                /* MPFR_MAX_EMBEDDED_PRECISION */
    emit_case(out, "edge", 249);                /* one above -> extended encoding, size=1 */
    emit_case(out, "edge", 250);
    emit_case(out, "edge", 255);
    emit_case(out, "edge", 256);
    emit_case(out, "edge", 504);                /* 248 + 256 boundary check */
    emit_case(out, "edge", 505);                /* size=2 starts here (504+1 = 505 - 248 - 1 = 256, 1 byte) */
    /* Boundary between 1- and 2-byte payload: 249 + 255 = 504 (1 byte covers 0..255). */
    emit_case(out, "edge", 503);                /* p - 249 = 254, fits in 1 byte */
    emit_case(out, "edge", 504);                /* p - 249 = 255, fits in 1 byte */
    /* p-249 = 256 needs 2 bytes => p = 505. */
    emit_case(out, "edge", 65784);              /* p-249 = 65535, fits 2 bytes */
    emit_case(out, "edge", 65785);              /* p-249 = 65536, needs 3 bytes */
    emit_case(out, "edge", 16777216);           /* p-249 = 2^24 - 249 boundary territory */
    emit_case(out, "edge", 16777465);           /* p-249 = 16777216 = 2^24, needs 4 bytes */
    emit_case(out, "edge", 16777464);           /* p-249 = 2^24 - 1, fits 3 bytes */
    emit_case(out, "edge", 4294967545ULL);      /* p-249 = 2^32 - 1 + 1 = 2^32, needs 5 bytes */
    emit_case(out, "edge", 4294967544ULL);      /* p-249 = 2^32 - 1, fits 4 bytes */
    emit_case(out, "edge", 9);
    emit_case(out, "edge", 16);
    emit_case(out, "edge", 32);
    emit_case(out, "edge", 33);
    emit_case(out, "edge", 63);
    emit_case(out, "edge", 65);
    emit_case(out, "edge", 127);
    emit_case(out, "edge", 129);
    emit_case(out, "edge", 1000000);
    emit_case(out, "edge", 100000000);

    /* adversarial: 12 -- around byte-boundaries and large values. */
    emit_case(out, "adversarial", 1099511627776ULL);          /* p-249 ~ 2^40 */
    emit_case(out, "adversarial", 281474976710656ULL);        /* p-249 ~ 2^48 */
    emit_case(out, "adversarial", 72057594037927936ULL);      /* p-249 ~ 2^56 */
    emit_case(out, "adversarial", 18446744073709551367ULL);   /* p-249 = UINT64_MAX - 248, 8 bytes */
    emit_case(out, "adversarial", 549755813632ULL);           /* boundary 5/6 byte */
    emit_case(out, "adversarial", 140737488355328ULL);        /* boundary 6/7 byte */
    emit_case(out, "adversarial", 36028797018963968ULL);      /* boundary 7/8 byte */
    emit_case(out, "adversarial", 250);
    emit_case(out, "adversarial", 251);
    emit_case(out, "adversarial", 252);
    emit_case(out, "adversarial", 4294967296ULL);             /* p-249 ~ 2^32 */
    emit_case(out, "adversarial", 4294967295ULL);

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xFEEDFACECAFEBABEULL);
        for (int rep = 0; rep < 50; ++rep) {
            uint64_t mode = xs64_below(&rng, 4);
            uint64_t precision;
            if (mode == 0) {
                /* embedded range */
                precision = 1 + xs64_below(&rng, MPFR_MAX_EMBEDDED_PRECISION);
            } else if (mode == 1) {
                /* small extended (size=1-2) */
                precision = (MPFR_MAX_EMBEDDED_PRECISION + 1) + xs64_below(&rng, 65536);
            } else if (mode == 2) {
                /* medium extended (size=3-4) */
                precision = (MPFR_MAX_EMBEDDED_PRECISION + 1) + xs64_below(&rng, 1ULL << 32);
            } else {
                /* large extended (size up to 8) */
                uint64_t r = xs64_next(&rng);
                /* avoid overflow on p-249 = UINT64_MAX bound */
                if (r > (uint64_t)-1 - (MPFR_MAX_EMBEDDED_PRECISION + 1)) {
                    r = (uint64_t)-1 - (MPFR_MAX_EMBEDDED_PRECISION + 1);
                }
                precision = (MPFR_MAX_EMBEDDED_PRECISION + 1) + r;
            }
            emit_case(out, "fuzz", precision);
        }
    }

    /* mined: 5 -- precisions exercised by mpfr/tests/tfpif.c. */
    emit_case(out, "mined", 130);    /* tfpif.c p1=130 (line 545) */
    emit_case(out, "mined", 2048);   /* tfpif.c p2=2048 (line 545) */
    emit_case(out, "mined", 1);      /* tfpif.c p1=1 in second fh_doit call */
    emit_case(out, "mined", 53);     /* tfpif.c p2=53 */
    emit_case(out, "mined", 17);     /* tfpif.c fh_extra: mpfr_init2(x, 17) */

    return 0;
}
