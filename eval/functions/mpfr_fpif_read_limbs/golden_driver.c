/*
 * golden_driver.c -- Golden master for mpfr_fpif_read_limbs.
 *
 * The C function is `static` in mpfr/src/fpif.c (L520-L542). We
 * re-implement the algorithm (golden-driver-substitute pattern per
 * ADR 0002) using GMP big-int byte handling. Driver and TS port
 * mirror the same substituted algorithm.
 *
 * The driver generates byte buffers by INVERTING mpfr_fpif_store_limbs
 * (re-used inline): for a given (prec, mant) we emit the canonical
 * byte stream, then read it back and emit the case.
 *
 * Wire format (per ADR 0004):
 *   inputs: { "bytes_value": "<decimal-bigint>",
 *             "byte_length": <int>,
 *             "pos": <int>,
 *             "prec": "<decimal>" }
 *   output: { "mant": "<decimal-bigint>", "nextPos": <int> }
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * Ref: mpfr/src/fpif.c L520-L542 -- C function body.
 * Ref: docs/adr/0004-binary-io-api.md -- API + wire ADR.
 */
#include "common.h"
#include <assert.h>
#include <gmp.h>
#include <inttypes.h>
#include <string.h>

#define MAX_BYTES 8192

/* Encode mantissa (MSB-aligned to prec) into nb_byte little-endian bytes.
 * Mirrors the simplified equivalent of fpif.c L483-L511 used by the
 * store_limbs driver. */
static size_t encode_mant(unsigned char *out, uint64_t prec, mpz_srcptr mant) {
    size_t nb_byte = (size_t)((prec + 7) / 8);
    size_t pad_bits = nb_byte * 8 - prec;
    mpz_t m;
    mpz_init(m);
    mpz_set(m, mant);
    if (pad_bits) mpz_mul_2exp(m, m, pad_bits);
    for (size_t i = 0; i < nb_byte; ++i) {
        out[i] = (unsigned char)(mpz_fdiv_ui(m, 256));
        mpz_fdiv_q_2exp(m, m, 8);
    }
    mpz_clear(m);
    return nb_byte;
}

/* Decode nb_byte bytes -> mant. Mirrors fpif.c L520-L542 with the
 * host-endian abstraction collapsed to LE. */
static void decode_mant(mpz_t mant_out, const unsigned char *bytes,
                         size_t pos, uint64_t prec) {
    size_t nb_byte = (size_t)((prec + 7) / 8);
    size_t pad_bits = nb_byte * 8 - prec;
    mpz_set_ui(mant_out, 0);
    for (size_t i = nb_byte; i > 0; --i) {
        mpz_mul_2exp(mant_out, mant_out, 8);
        mpz_add_ui(mant_out, mant_out, bytes[pos + i - 1]);
    }
    if (pad_bits) mpz_fdiv_q_2exp(mant_out, mant_out, pad_bits);
}

/* Generate a valid mantissa with bit (prec-1) set, all bits >= prec
 * clear -- i.e. mant in [2^(prec-1), 2^prec). */
static void gen_mant(mpz_t out, uint64_t prec, xs64_t *rng) {
    mpz_set_ui(out, 0);
    /* Random low bits + force MSB. */
    /* Build a random integer in [0, 2^(prec-1)) by streaming xs64 limbs. */
    uint64_t bits_left = prec - 1;
    mpz_t limb;
    mpz_init(limb);
    while (bits_left > 0) {
        uint64_t chunk = bits_left >= 64 ? 64 : bits_left;
        uint64_t v = xs64_next(rng);
        if (chunk < 64) v &= ((uint64_t)1 << chunk) - 1;
        mpz_set_ui(limb, v >> 32);
        mpz_mul_2exp(limb, limb, 32);
        mpz_add_ui(limb, limb, v & 0xFFFFFFFF);
        if (chunk < 64) {
            /* keep only the bottom `chunk` bits */
            mpz_fdiv_r_2exp(limb, limb, chunk);
        }
        mpz_mul_2exp(out, out, chunk);
        mpz_add(out, out, limb);
        bits_left -= chunk;
    }
    mpz_clear(limb);
    /* Set bit (prec-1). */
    mpz_setbit(out, prec - 1);
}

static void bytes_to_mpz(mpz_t z, const unsigned char *buf, size_t n) {
    mpz_set_ui(z, 0);
    for (size_t i = n; i > 0; --i) {
        mpz_mul_2exp(z, z, 8);
        mpz_add_ui(z, z, buf[i - 1]);
    }
}

static void emit_case_with_mant(FILE *out, const char *tag,
                                 uint64_t prec, mpz_srcptr mant,
                                 size_t prefix_len) {
    unsigned char buf[MAX_BYTES] = {0};
    size_t nb_byte = (size_t)((prec + 7) / 8);
    assert(prefix_len + nb_byte <= MAX_BYTES);
    encode_mant(buf + prefix_len, prec, mant);
    size_t buf_len = prefix_len + nb_byte;
    size_t pos = prefix_len;

    mpz_t got;
    mpz_init(got);
    const uint64_t t0 = now_ns();
    decode_mant(got, buf, pos, prec);
    const uint64_t elapsed = now_ns() - t0;
    assert(mpz_cmp(got, mant) == 0);  /* round-trip identity */

    mpz_t bytes_z;
    mpz_init(bytes_z);
    bytes_to_mpz(bytes_z, buf, buf_len);
    char *bytes_str = mpz_get_str(NULL, 10, bytes_z);
    char *mant_str = mpz_get_str(NULL, 10, got);

    jl_begin(out, tag);
    fprintf(out, "\"bytes_value\":\"%s\"", bytes_str);
    fprintf(out, ",\"byte_length\":%zu", buf_len);
    fprintf(out, ",\"pos\":%zu", pos);
    fprintf(out, ",\"prec\":\"%" PRIu64 "\"", prec);
    jl_end_inputs(out);

    fprintf(out, ",\"output\":{\"mant\":\"%s\",\"nextPos\":%zu}",
            mant_str, pos + nb_byte);
    jl_finish(out, elapsed);

    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(bytes_str, strlen(bytes_str) + 1);
    gmp_free(mant_str, strlen(mant_str) + 1);
    mpz_clear(bytes_z);
    mpz_clear(got);
}

/* Helper: emit case for prec with mant set to 2^(prec-1) (MSB only). */
static void emit_msb_only(FILE *out, const char *tag, uint64_t prec, size_t prefix_len) {
    mpz_t m;
    mpz_init(m);
    mpz_set_ui(m, 0);
    mpz_setbit(m, prec - 1);
    emit_case_with_mant(out, tag, prec, m, prefix_len);
    mpz_clear(m);
}

/* Helper: emit case for prec with mant = 2^prec - 1 (all ones). */
static void emit_all_ones(FILE *out, const char *tag, uint64_t prec, size_t prefix_len) {
    mpz_t m;
    mpz_init(m);
    mpz_set_ui(m, 1);
    mpz_mul_2exp(m, m, prec);
    mpz_sub_ui(m, m, 1);
    emit_case_with_mant(out, tag, prec, m, prefix_len);
    mpz_clear(m);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- common precs, MSB-only mantissa. */
    {
        const uint64_t precs[20] = {
            1, 2, 8, 16, 24, 32, 53, 64, 65, 100,
            113, 128, 130, 200, 256, 512, 1000, 1024, 2000, 2048,
        };
        for (int i = 0; i < 20; ++i) emit_msb_only(out, "happy", precs[i], 0);
    }

    /* edge: 30 -- boundaries. */
    emit_msb_only(out, "edge", 1, 0);
    emit_all_ones(out, "edge", 1, 0);
    emit_msb_only(out, "edge", 7, 0);
    emit_all_ones(out, "edge", 7, 0);
    emit_msb_only(out, "edge", 8, 0);
    emit_all_ones(out, "edge", 8, 0);
    emit_msb_only(out, "edge", 9, 0);
    emit_all_ones(out, "edge", 9, 0);
    emit_msb_only(out, "edge", 64, 0);
    emit_all_ones(out, "edge", 64, 0);
    emit_msb_only(out, "edge", 65, 0);
    emit_all_ones(out, "edge", 65, 0);
    emit_msb_only(out, "edge", 128, 0);
    emit_all_ones(out, "edge", 128, 0);
    emit_msb_only(out, "edge", 129, 0);
    emit_all_ones(out, "edge", 129, 0);
    /* pos > 0 cursor exercises. */
    emit_msb_only(out, "edge", 53, 1);
    emit_msb_only(out, "edge", 53, 5);
    emit_msb_only(out, "edge", 130, 7);
    emit_msb_only(out, "edge", 1024, 3);
    /* Various odd precs. */
    emit_all_ones(out, "edge", 3, 0);
    emit_all_ones(out, "edge", 5, 0);
    emit_all_ones(out, "edge", 17, 0);
    emit_all_ones(out, "edge", 33, 0);
    emit_all_ones(out, "edge", 100, 0);
    emit_all_ones(out, "edge", 200, 0);
    /* Precs that are multiples of 8 (no partial byte). */
    emit_msb_only(out, "edge", 256, 0);
    emit_all_ones(out, "edge", 256, 0);
    emit_msb_only(out, "edge", 512, 0);
    emit_msb_only(out, "edge", 1024, 0);

    /* adversarial: 12 -- larger precs and random mantissas. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xDEADBEEF12345678ULL);
        const uint64_t precs[12] = {
            100, 500, 1000, 2000, 3000, 4096,
            117, 257, 521, 1023, 2049, 4095,
        };
        for (int i = 0; i < 12; ++i) {
            mpz_t m;
            mpz_init(m);
            gen_mant(m, precs[i], &rng);
            emit_case_with_mant(out, "adversarial", precs[i], m, (size_t)(i % 8));
            mpz_clear(m);
        }
    }

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xBEEFDEAD13379001ULL);
        for (int rep = 0; rep < 50; ++rep) {
            uint64_t prec = 1 + xs64_below(&rng, 2048);
            size_t prefix = (size_t)xs64_below(&rng, 16);
            mpz_t m;
            mpz_init(m);
            gen_mant(m, prec, &rng);
            emit_case_with_mant(out, "fuzz", prec, m, prefix);
            mpz_clear(m);
        }
    }

    /* mined: 5 -- (prec, mant) pairs from mpfr/tests/tfpif.c. */
    /* Use libmpfr to derive a faithful mantissa for the test values. */
    {
        mpfr_t x;
        mpz_t mant;
        mpz_init(mant);
        mpfr_init2(x, 130);
        mpfr_set_str(x, "45.2564215000000018562786863185465335845947265625", 10, MPFR_RNDN);
        (void) mpfr_get_z_2exp(mant, x);
        mpz_abs(mant, mant);
        emit_case_with_mant(out, "mined", 130, mant, 0);
        mpfr_clear(x);

        mpfr_init2(x, 2048);
        mpfr_set_str(x, "45.2564215000000018562786863185465335845947265625", 10, MPFR_RNDN);
        (void) mpfr_get_z_2exp(mant, x);
        mpz_abs(mant, mant);
        emit_case_with_mant(out, "mined", 2048, mant, 0);
        mpfr_clear(x);

        mpfr_init2(x, 2048);
        mpfr_set_ui(x, 104348, MPFR_RNDN);
        (void) mpfr_get_z_2exp(mant, x);
        mpz_abs(mant, mant);
        emit_case_with_mant(out, "mined", 2048, mant, 0);
        mpfr_clear(x);

        mpfr_init2(x, 53);
        mpfr_set_str(x, "45.2564215000000018562786863185465335845947265625", 10, MPFR_RNDN);
        (void) mpfr_get_z_2exp(mant, x);
        mpz_abs(mant, mant);
        emit_case_with_mant(out, "mined", 53, mant, 0);
        mpfr_clear(x);

        mpfr_init2(x, 17);
        mpfr_set_ui(x, 42, MPFR_RNDN);
        (void) mpfr_get_z_2exp(mant, x);
        mpz_abs(mant, mant);
        emit_case_with_mant(out, "mined", 17, mant, 0);
        mpfr_clear(x);

        mpz_clear(mant);
    }

    return 0;
}
