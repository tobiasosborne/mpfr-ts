/*
 * golden_driver.c -- Golden master for mpfr_fpif_store_limbs.
 *
 * The C function is `static` in mpfr/src/fpif.c (L483-L511). We
 * re-implement the algorithm (golden-driver-substitute pattern per
 * ADR 0002) using GMP-derived mantissa extraction + portable LE
 * byte writing. Both the driver and the TS port mirror the same
 * substituted algorithm, so strict-equality grading is sound.
 *
 * Algorithm
 * ---------
 *
 *   1. Extract the MSB-aligned mantissa as a bignum z (via
 *      mpfr_get_z_2exp + abs). This is identical to MPFR's TS schema
 *      x.mant per src/core.ts.
 *   2. nb_byte = ceil(prec / 8); pad_bits = nb_byte * 8 - prec.
 *   3. Shift z left by pad_bits to recover the MSB-aligned padded
 *      limb layout the C code writes.
 *   4. Write nb_byte bytes little-endian.
 *
 * This produces the same byte sequence the C function writes via its
 * partial-byte + full-LE-limbs path (the equivalence falls out of the
 * format spec at fpif.c L59-L60 once host endianness is fixed to LE).
 *
 * Wire format (per ADR 0004):
 *   inputs: { "x": <MPFR wire, kind=normal> }
 *   output: { "bytes": "<decimal-bigint>", "byte_length": <int> }
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * Ref: mpfr/src/fpif.c L483-L511 -- C function body.
 * Ref: mpfr/src/fpif.c L59-L60 -- significand field format spec.
 * Ref: docs/adr/0004-binary-io-api.md -- API + wire ADR.
 */
#include "common.h"
#include <assert.h>
#include <gmp.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

/* Encode the mantissa of a regular MPFR value into the fpif byte stream.
 * Returns nb_byte = ceil(prec / 8). The out buffer must be at least
 * nb_byte bytes long. */
static size_t encode_limbs(unsigned char *out, mpfr_srcptr x) {
    assert(mpfr_regular_p(x));
    const mpfr_prec_t prec = mpfr_get_prec(x);
    const size_t nb_byte = (size_t)((prec + 7) >> 3);
    const size_t pad_bits = nb_byte * 8 - (size_t)prec;

    /* Extract MSB-aligned mantissa via mpfr_get_z_2exp + abs (matches
     * src/core.ts jl_kv_mpfr -- the same Option A used by the value
     * emitter; both sides see exactly the same mant value). */
    mpz_t z;
    mpz_init(z);
    (void) mpfr_get_z_2exp(z, x);
    mpz_abs(z, z);

    /* Shift up by pad_bits to recover the MPFR_MANT padded layout. */
    if (pad_bits != 0) {
        mpz_mul_2exp(z, z, (mp_bitcnt_t)pad_bits);
    }

    /* Write nb_byte little-endian bytes. We could use mpz_export but
     * little-endian byte extraction by repeated mod/div keeps the code
     * dependency-free and trivially auditable. */
    mpz_t mod, q, tmp;
    mpz_inits(mod, q, tmp, (mpz_ptr) 0);
    for (size_t i = 0; i < nb_byte; ++i) {
        mpz_fdiv_r_2exp(mod, z, 8);            /* z mod 256 */
        out[i] = (unsigned char)mpz_get_ui(mod);
        mpz_fdiv_q_2exp(z, z, 8);              /* z /= 256 */
    }
    mpz_clears(mod, q, tmp, (mpz_ptr) 0);
    mpz_clear(z);
    return nb_byte;
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

#define MAX_BYTES 4096

static void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    unsigned char buf[MAX_BYTES] = {0};
    const uint64_t t0 = now_ns();
    size_t n = encode_limbs(buf, x);
    const uint64_t elapsed = now_ns() - t0;
    assert(n <= MAX_BYTES);
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    emit_output(out, buf, n);
    jl_finish(out, elapsed);
}

/* Build an MPFR normal value at given prec with a mantissa derived
 * from a 64-bit seed (deterministic). */
static void mk_normal_from_seed(mpfr_t x, mpfr_prec_t prec, uint64_t seed, int sign) {
    mpfr_set_prec(x, prec);
    /* Use the low bits as an unsigned long mantissa carrier. */
    unsigned long ulval = (unsigned long)((seed | 1ULL) & ULONG_MAX);
    if (ulval == 0) ulval = 1;
    mpfr_set_ui(x, ulval, MPFR_RNDN);
    if (sign < 0) mpfr_neg(x, x, MPFR_RNDN);
}

int main(void) {
    FILE *out = stdout;
    mpfr_t x;
    mpfr_init2(x, 53);

    /* happy: 20 -- common precisions, exact integer values. */
    {
        const mpfr_prec_t precs[20] = {
            1, 2, 8, 16, 24, 32, 53, 64, 65, 100,
            113, 128, 130, 200, 256, 512, 1000, 1024, 2000, 2048,
        };
        for (int i = 0; i < 20; ++i) {
            mpfr_set_prec(x, precs[i]);
            mpfr_set_ui(x, (unsigned long)(i + 1), MPFR_RNDN);
            emit_case(out, "happy", x);
        }
    }

    /* edge: 30 -- boundary cases. */
    /* Single-bit (prec=1) values. */
    mpfr_set_prec(x, 1); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 1); mpfr_set_si(x, -1, MPFR_RNDN); emit_case(out, "edge", x);
    /* Precisions that are multiples of 8 (no partial byte). */
    mpfr_set_prec(x, 8); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 16); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 64); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 128); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    /* Precisions with a 1-byte partial. */
    mpfr_set_prec(x, 9); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 17); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 65); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 129); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    /* Precisions with a 7-byte partial. */
    mpfr_set_prec(x, 7); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 15); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 63); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 71); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 127); mpfr_set_ui(x, 1, MPFR_RNDN); emit_case(out, "edge", x);
    /* All-ones mantissa: mpfr_nextbelow(2^p) = 2^p - 1 ulp = (2^p - 1) * 2^(exp-p). */
    mpfr_set_prec(x, 8); mpfr_set_ui(x, 255, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 16); mpfr_set_ui(x, 65535, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 53); mpfr_set_d(x, 1.5, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 53); mpfr_set_d(x, 1e100, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 53); mpfr_set_d(x, 1e-100, MPFR_RNDN); emit_case(out, "edge", x);
    /* Negative values. */
    mpfr_set_prec(x, 53); mpfr_set_si(x, -42, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 64); mpfr_set_si(x, -65535, MPFR_RNDN); emit_case(out, "edge", x);
    /* Boundary at prec=248 (last short embedded precision). */
    mpfr_set_prec(x, 248); mpfr_set_ui(x, 7, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 249); mpfr_set_ui(x, 7, MPFR_RNDN); emit_case(out, "edge", x);
    /* Boundary at prec=255, 256, 257. */
    mpfr_set_prec(x, 255); mpfr_set_ui(x, 11, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 256); mpfr_set_ui(x, 11, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 257); mpfr_set_ui(x, 11, MPFR_RNDN); emit_case(out, "edge", x);
    /* Multiple of 8 large. */
    mpfr_set_prec(x, 1024); mpfr_set_d(x, 1.5, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 2048); mpfr_set_d(x, 3.14159, MPFR_RNDN); emit_case(out, "edge", x);
    /* Small odd prec. */
    mpfr_set_prec(x, 3); mpfr_set_ui(x, 5, MPFR_RNDN); emit_case(out, "edge", x);
    mpfr_set_prec(x, 5); mpfr_set_ui(x, 17, MPFR_RNDN); emit_case(out, "edge", x);

    /* adversarial: 12 -- large precs and corner mantissa patterns. */
    /* Mantissa pattern: exactly the MSB set (power of 2). */
    mpfr_set_prec(x, 17); mpfr_set_ui_2exp(x, 1, 16, MPFR_RNDN); emit_case(out, "adversarial", x);
    mpfr_set_prec(x, 130); mpfr_set_ui_2exp(x, 1, 100, MPFR_RNDN); emit_case(out, "adversarial", x);
    /* All-ones mantissa just under power of 2. */
    mpfr_set_prec(x, 11); mpfr_set_ui(x, (1u << 11) - 1, MPFR_RNDN); emit_case(out, "adversarial", x);
    mpfr_set_prec(x, 25); mpfr_set_ui(x, (1u << 25) - 1, MPFR_RNDN); emit_case(out, "adversarial", x);
    /* Large precs. */
    mpfr_set_prec(x, 4000); mpfr_set_d(x, 1.0/3.0, MPFR_RNDN); emit_case(out, "adversarial", x);
    mpfr_set_prec(x, 8000); mpfr_set_d(x, 1.0/7.0, MPFR_RNDN); emit_case(out, "adversarial", x);
    /* Various odd precs. */
    mpfr_set_prec(x, 33); mpfr_set_d(x, 3.14159, MPFR_RNDN); emit_case(out, "adversarial", x);
    mpfr_set_prec(x, 99); mpfr_set_d(x, 2.71828, MPFR_RNDN); emit_case(out, "adversarial", x);
    mpfr_set_prec(x, 137); mpfr_set_d(x, 1.41421, MPFR_RNDN); emit_case(out, "adversarial", x);
    mpfr_set_prec(x, 521); mpfr_set_d(x, 1.61803, MPFR_RNDN); emit_case(out, "adversarial", x);
    mpfr_set_prec(x, 1000); mpfr_set_d(x, 1e-50, MPFR_RNDN); emit_case(out, "adversarial", x);
    mpfr_set_prec(x, 1023); mpfr_set_d(x, 1e50, MPFR_RNDN); emit_case(out, "adversarial", x);

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xCAFED00DDEADBEEFULL);
        for (int rep = 0; rep < 50; ++rep) {
            /* prec in [1, 2048] biased toward smaller. */
            mpfr_prec_t prec = (mpfr_prec_t)(1 + xs64_below(&rng, 2048));
            uint64_t v = xs64_next(&rng);
            if (v == 0) v = 1;  /* avoid zero (we want normal kind) */
            int sign = (xs64_below(&rng, 2)) ? -1 : 1;
            mk_normal_from_seed(x, prec, v, sign);
            emit_case(out, "fuzz", x);
        }
    }

    /* mined: 5 -- precisions/values from mpfr/tests/tfpif.c. */
    /* The set_str1 "45.2564..." at prec 130. */
    mpfr_set_prec(x, 130);
    mpfr_set_str(x, "45.2564215000000018562786863185465335845947265625", 10, MPFR_RNDN);
    emit_case(out, "mined", x);
    /* Same string at prec 2048. */
    mpfr_set_prec(x, 2048);
    mpfr_set_str(x, "45.2564215000000018562786863185465335845947265625", 10, MPFR_RNDN);
    emit_case(out, "mined", x);
    /* mpfr_set_ui(x[6], 104348, RNDN) at prec=2048. */
    mpfr_set_prec(x, 2048);
    mpfr_set_ui(x, 104348, MPFR_RNDN);
    emit_case(out, "mined", x);
    /* Result of 104348/33215 at prec=2048 -- a long fractional. */
    {
        mpfr_t a, b;
        mpfr_init2(a, 2048);
        mpfr_init2(b, 2048);
        mpfr_set_ui(a, 104348, MPFR_RNDN);
        mpfr_set_ui(b, 33215, MPFR_RNDN);
        mpfr_div(x, a, b, MPFR_RNDN);
        mpfr_clear(a); mpfr_clear(b);
    }
    emit_case(out, "mined", x);
    /* mpfr_set_ui(x, 42, RNDN) at prec=17 (tfpif.c fh_extra). */
    mpfr_set_prec(x, 17);
    mpfr_set_ui(x, 42, MPFR_RNDN);
    emit_case(out, "mined", x);

    mpfr_clear(x);
    return 0;
}
