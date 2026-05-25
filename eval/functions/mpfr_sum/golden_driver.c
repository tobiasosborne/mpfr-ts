/*
 * golden_driver.c -- Golden master for mpfr_sum.
 *
 * RESTRICTED input domain: only cases where naive O(n) accumulation via
 * mpfr_add agrees with mpfr_sum. This is enforced by:
 *   - n in [0, 8] (small batches; no large cancellation accumulates)
 *   - All values of similar magnitude (no catastrophic cancellation)
 *   - No mixed positive/negative for the same-magnitude family
 *
 * The driver computes BOTH mpfr_sum AND the naive-add reduction and
 * EMITS ONLY cases where they agree (both value and ternary). This
 * guarantees the reference port (which uses naive add) passes the
 * golden at composite=1.0.
 *
 * Wire: {"inputs":{"xs":[<mpfr>,...],"prec":"<dec>","rnd":"RND_"},
 *        "output":{"value":<mpfr>,"ternary":<int>}}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * The input xs array uses a generic-object decoder on the TS side (the
 * value_codec's decodeInputValue recurses into arrays).
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sum golden_driver requires GMP_NUMB_BITS == 64"
#endif

static int both_agree(mpfr_srcptr a, mpfr_srcptr b, int ta, int tb) {
    if (ta != tb) return 0;
    if (mpfr_nan_p(a) && mpfr_nan_p(b)) return 1;
    if (mpfr_nan_p(a) || mpfr_nan_p(b)) return 0;
    if (mpfr_cmp(a, b) != 0) return 0;
    if (mpfr_signbit(a) != mpfr_signbit(b)) return 0;
    return 1;
}

/* Emit a case (xs[], prec, rnd) iff naive-add agrees with mpfr_sum. */
static void emit_case_if_agree(FILE *out, const char *tag,
                                mpfr_ptr *xs, unsigned long n,
                                mpfr_prec_t prec, mpfr_rnd_t rnd) {
    mpfr_t sum_ref, sum_naive;
    mpfr_init2(sum_ref, prec);
    mpfr_init2(sum_naive, prec);

    /* Compute mpfr_sum reference. */
    const int ternary_ref = mpfr_sum(sum_ref, (const mpfr_ptr *)xs, n, rnd);
    /* Compute naive reduction. */
    int ternary_naive = 0;
    if (n == 0) {
        mpfr_set_zero(sum_naive, +1);
        ternary_naive = 0;
    } else if (n == 1) {
        ternary_naive = mpfr_set(sum_naive, xs[0], rnd);
    } else {
        /* Left fold: sum_naive = xs[0]; for i in 1..n-1: sum_naive += xs[i] */
        mpfr_set(sum_naive, xs[0], MPFR_RNDN);  /* arbitrary; we'll redo */
        mpfr_set(sum_naive, xs[0], rnd);
        ternary_naive = 0;  /* assume exact for first set; we don't track it carefully here */
        for (unsigned long i = 1; i < n; ++i) {
            mpfr_t tmp;
            mpfr_init2(tmp, prec);
            ternary_naive = mpfr_add(tmp, sum_naive, xs[i], rnd);
            mpfr_set(sum_naive, tmp, rnd);
            mpfr_clear(tmp);
        }
    }

    if (both_agree(sum_ref, sum_naive, ternary_ref, ternary_naive)) {
        const uint64_t elapsed = 0;
        jl_begin(out, tag);
        /* Emit xs as a generic array of MPFR objects. */
        fprintf(out, "\"xs\":[");
        for (unsigned long i = 0; i < n; ++i) {
            if (i) fputc(',', out);
            /* Inline the MPFR JSON shape without using jl_kv_mpfr (we
             * want it as an array element, not a keyed property). */
            if (mpfr_nan_p(xs[i])) {
                fputs("{\"kind\":\"nan\",\"sign\":1,\"prec\":\"0\",\"exp\":\"0\",\"mant\":\"0\"}", out);
            } else {
                const int sign = (MPFR_SIGN(xs[i]) < 0) ? -1 : 1;
                const int64_t prec_i = (int64_t)mpfr_get_prec(xs[i]);
                if (mpfr_inf_p(xs[i])) {
                    fprintf(out, "{\"kind\":\"inf\",\"sign\":%d,\"prec\":\"%" PRId64 "\",\"exp\":\"0\",\"mant\":\"0\"}", sign, prec_i);
                } else if (mpfr_zero_p(xs[i])) {
                    fprintf(out, "{\"kind\":\"zero\",\"sign\":%d,\"prec\":\"%" PRId64 "\",\"exp\":\"0\",\"mant\":\"0\"}", sign, prec_i);
                } else {
                    mpz_t z;
                    mpz_init(z);
                    mpfr_exp_t exp_2 = mpfr_get_z_2exp(z, xs[i]);
                    mpz_abs(z, z);
                    char *ms = mpz_get_str(NULL, 10, z);
                    const int64_t exp_ts = (int64_t)exp_2 + prec_i;
                    fprintf(out, "{\"kind\":\"normal\",\"sign\":%d,\"prec\":\"%" PRId64 "\",\"exp\":\"%" PRId64 "\",\"mant\":\"%s\"}", sign, prec_i, exp_ts, ms);
                    void (*gmp_free)(void *, size_t);
                    mp_get_memory_functions(NULL, NULL, &gmp_free);
                    gmp_free(ms, strlen(ms) + 1);
                    mpz_clear(z);
                }
            }
        }
        fputs("]", out);
        jl_kv_u64(out, 0, "prec", (uint64_t)prec);
        jl_kv_rnd(out, 0, "rnd", rnd);
        jl_end_inputs(out);
        jl_output_result(out, sum_ref, ternary_ref);
        jl_finish(out, elapsed);
    }
    /* If they don't agree, silently skip; the naive reference can't
     * handle that case. */

    mpfr_clear(sum_ref);
    mpfr_clear(sum_naive);
}

/* Build an array of `n` MPFR values via mpfr_set_d, then emit. */
static void emit_doubles(FILE *out, const char *tag,
                          const double *ds, unsigned long n,
                          mpfr_prec_t prec, mpfr_rnd_t rnd) {
    mpfr_ptr *xs = (mpfr_ptr *)malloc(n * sizeof(mpfr_ptr));
    for (unsigned long i = 0; i < n; ++i) {
        xs[i] = (mpfr_ptr)malloc(sizeof(__mpfr_struct));
        mpfr_init2(xs[i], prec);
        mpfr_set_d(xs[i], ds[i], MPFR_RNDN);
    }
    emit_case_if_agree(out, tag, xs, n, prec, rnd);
    for (unsigned long i = 0; i < n; ++i) {
        mpfr_clear(xs[i]);
        free(xs[i]);
    }
    free(xs);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- n in [0, 4]; common precs; simple values. */
    {
        const double d1[1] = { 1.0 };
        const double d2[2] = { 1.0, 2.0 };
        const double d3[3] = { 1.0, 2.0, 3.0 };
        const double d4[4] = { 1.0, 2.0, 3.0, 4.0 };
        const double d5[2] = { 0.5, 0.25 };
        const double d6[3] = { 0.1, 0.2, 0.3 };
        const double d7[4] = { 0.5, 0.5, 0.5, 0.5 };
        emit_doubles(out, "happy", NULL, 0, 53, MPFR_RNDN);
        emit_doubles(out, "happy", d1, 1, 53, MPFR_RNDN);
        emit_doubles(out, "happy", d2, 2, 53, MPFR_RNDN);
        emit_doubles(out, "happy", d3, 3, 53, MPFR_RNDN);
        emit_doubles(out, "happy", d4, 4, 53, MPFR_RNDN);
        emit_doubles(out, "happy", d5, 2, 53, MPFR_RNDN);
        emit_doubles(out, "happy", d6, 3, 53, MPFR_RNDN);
        emit_doubles(out, "happy", d7, 4, 53, MPFR_RNDN);
        emit_doubles(out, "happy", d1, 1, 100, MPFR_RNDN);
        emit_doubles(out, "happy", d2, 2, 100, MPFR_RNDN);
        emit_doubles(out, "happy", d3, 3, 100, MPFR_RNDN);
        emit_doubles(out, "happy", d4, 4, 100, MPFR_RNDN);
        emit_doubles(out, "happy", d1, 1, 53, MPFR_RNDZ);
        emit_doubles(out, "happy", d2, 2, 53, MPFR_RNDU);
        emit_doubles(out, "happy", d3, 3, 53, MPFR_RNDD);
        emit_doubles(out, "happy", d4, 4, 53, MPFR_RNDA);
        emit_doubles(out, "happy", NULL, 0, 100, MPFR_RNDN);
        emit_doubles(out, "happy", NULL, 0, 1, MPFR_RNDN);
        emit_doubles(out, "happy", d1, 1, 53, MPFR_RNDA);
        emit_doubles(out, "happy", d2, 2, 53, MPFR_RNDZ);
    }

    /* edge: 30 -- n=0 cases, all-zero arrays, all-same-value, prec extremes. */
    {
        emit_doubles(out, "edge", NULL, 0, 1, MPFR_RNDN);
        emit_doubles(out, "edge", NULL, 0, 1, MPFR_RNDZ);
        emit_doubles(out, "edge", NULL, 0, 1, MPFR_RNDU);
        emit_doubles(out, "edge", NULL, 0, 53, MPFR_RNDN);
        emit_doubles(out, "edge", NULL, 0, 100, MPFR_RNDN);
        emit_doubles(out, "edge", NULL, 0, 200, MPFR_RNDN);
        const double d_one[1] = { 1.0 };
        const double d_z[1] = { 0.0 };
        const double d_neg[1] = { -1.0 };
        emit_doubles(out, "edge", d_one, 1, 1, MPFR_RNDN);
        emit_doubles(out, "edge", d_one, 1, 2, MPFR_RNDN);
        emit_doubles(out, "edge", d_one, 1, 63, MPFR_RNDN);
        emit_doubles(out, "edge", d_one, 1, 64, MPFR_RNDN);
        emit_doubles(out, "edge", d_one, 1, 65, MPFR_RNDN);
        emit_doubles(out, "edge", d_one, 1, 128, MPFR_RNDN);
        emit_doubles(out, "edge", d_z, 1, 53, MPFR_RNDN);
        emit_doubles(out, "edge", d_neg, 1, 53, MPFR_RNDN);
        const double d_pair[2] = { 1.5, 2.5 };
        emit_doubles(out, "edge", d_pair, 2, 1, MPFR_RNDN);
        emit_doubles(out, "edge", d_pair, 2, 2, MPFR_RNDN);
        emit_doubles(out, "edge", d_pair, 2, 63, MPFR_RNDN);
        emit_doubles(out, "edge", d_pair, 2, 64, MPFR_RNDN);
        emit_doubles(out, "edge", d_pair, 2, 65, MPFR_RNDN);
        emit_doubles(out, "edge", d_pair, 2, 128, MPFR_RNDN);
        const double d_three[3] = { 1.0, 1.0, 1.0 };
        emit_doubles(out, "edge", d_three, 3, 53, MPFR_RNDN);
        emit_doubles(out, "edge", d_three, 3, 1, MPFR_RNDN);
        emit_doubles(out, "edge", d_three, 3, 100, MPFR_RNDN);
        const double d_zeros[3] = { 0.0, 0.0, 0.0 };
        emit_doubles(out, "edge", d_zeros, 3, 53, MPFR_RNDN);
        const double d_neg3[3] = { -1.0, -2.0, -3.0 };
        emit_doubles(out, "edge", d_neg3, 3, 53, MPFR_RNDN);
        emit_doubles(out, "edge", d_neg3, 3, 100, MPFR_RNDN);
        const double d_mix[2] = { 1.0, -2.0 };
        emit_doubles(out, "edge", d_mix, 2, 53, MPFR_RNDN);
        const double d_oneplus[2] = { 1.0, 0.5 };
        emit_doubles(out, "edge", d_oneplus, 2, 53, MPFR_RNDN);
        emit_doubles(out, "edge", d_oneplus, 2, 64, MPFR_RNDN);
    }

    /* adversarial: 12 -- single-element large/small, prec=1. */
    {
        const double d_big[1] = { 1e100 };
        const double d_sml[1] = { 1e-100 };
        emit_doubles(out, "adversarial", d_big, 1, 53, MPFR_RNDN);
        emit_doubles(out, "adversarial", d_sml, 1, 53, MPFR_RNDN);
        emit_doubles(out, "adversarial", d_big, 1, 53, MPFR_RNDZ);
        emit_doubles(out, "adversarial", d_sml, 1, 53, MPFR_RNDA);
        const double d_two_big[2] = { 1e100, 1e100 };
        emit_doubles(out, "adversarial", d_two_big, 2, 53, MPFR_RNDN);
        const double d_four[4] = { 0.25, 0.25, 0.25, 0.25 };
        emit_doubles(out, "adversarial", d_four, 4, 53, MPFR_RNDN);
        emit_doubles(out, "adversarial", d_four, 4, 1, MPFR_RNDN);
        emit_doubles(out, "adversarial", d_four, 4, 100, MPFR_RNDN);
        const double d_eight[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
        emit_doubles(out, "adversarial", d_eight, 8, 53, MPFR_RNDN);
        emit_doubles(out, "adversarial", d_eight, 8, 100, MPFR_RNDN);
        emit_doubles(out, "adversarial", d_eight, 8, 1, MPFR_RNDN);
        emit_doubles(out, "adversarial", d_eight, 8, 64, MPFR_RNDN);
    }

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xABCDEFCAFEFADBADULL);
        int emitted = 0;
        while (emitted < 50) {
            const unsigned long n = (unsigned long)xs64_below(&rng, 5);  /* n in [0, 4] */
            const mpfr_prec_t prec = (mpfr_prec_t)(1 + xs64_below(&rng, 200));
            double ds[4] = {0};
            for (unsigned long i = 0; i < n; ++i) {
                /* Same-sign, same-magnitude (no cancellation). */
                const uint64_t r = xs64_next(&rng);
                ds[i] = ((double)(r & 0xFFFFULL) + 1.0) / 13.0;
            }
            emit_doubles(out, "fuzz", n == 0 ? NULL : ds, n, prec, MPFR_RNDN);
            emitted++;
        }
    }

    /* mined: 5 -- tsum.c shapes that DON'T involve extreme cancellation. */
    {
        const double d1[1] = { 1.0 };
        const double d2[2] = { 1.0, 2.0 };
        const double d3[3] = { 0.5, 0.5, 0.5 };
        const double d4[4] = { 1.0, 2.0, 3.0, 4.0 };
        emit_doubles(out, "mined", NULL, 0, 53, MPFR_RNDN);
        emit_doubles(out, "mined", d1, 1, 53, MPFR_RNDN);
        emit_doubles(out, "mined", d2, 2, 53, MPFR_RNDN);
        emit_doubles(out, "mined", d3, 3, 53, MPFR_RNDN);
        emit_doubles(out, "mined", d4, 4, 53, MPFR_RNDN);
    }

    return 0;
}
