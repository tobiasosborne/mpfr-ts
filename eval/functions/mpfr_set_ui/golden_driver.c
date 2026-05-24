/*
 * golden_driver.c — Golden master for MPFR's mpfr_set_ui.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_ui(mpfr_t rop, unsigned long int op, mpfr_rnd_t rnd);
 *
 *   Stores `op` into `rop` rounded per `rnd`. Returns the ternary.
 *   Ref: mpfr/src/set_ui.c L25–L29 → mpfr/src/set_ui_2exp.c L26–L90.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"n":"<uint64-decimal>","prec":"<decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 *   - `n` is jl_kv_u64 (quoted unsigned decimal string, parsed as
 *     BigInt on the TS side).
 *
 * Tag distribution
 * ----------------
 *
 *   happy        :  ~25
 *   edge         :  ~50  (0, 1, ULONG_MAX, 2^k, 2^k - 1, prec boundaries)
 *   adversarial  :  ~30
 *   fuzz         :   55  (xs64 seed 0x0750D75007550750ULL)
 *   mined        :    5  (from mpfr/tests/tset_ui.c)
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_ui golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

_Static_assert(sizeof(unsigned long) == 8, "mpfr_set_ui golden requires 64-bit unsigned long");

static inline void emit_case(FILE *out, const char *tag,
                             unsigned long n, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_set_ui(rop, n, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_u64(out, 1, "n", (uint64_t)n);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25 cases.                                              */
    /* ============================================================== */
    {
        emit_case(out, "happy", 0UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 0UL, 24, MPFR_RNDN);
        emit_case(out, "happy", 0UL, 1, MPFR_RNDN);
        emit_case(out, "happy", 1UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 2UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 10UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 100UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 1000UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 1000000UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 1000000000UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 42UL, 24, MPFR_RNDN);
        emit_case(out, "happy", 42UL, 64, MPFR_RNDN);
        emit_case(out, "happy", 1234567UL, 100, MPFR_RNDN);
        emit_case(out, "happy", 3UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 5UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 7UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 255UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 256UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 65535UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 65536UL, 53, MPFR_RNDN);
        emit_case(out, "happy", 0xFFFFFFFFUL, 53, MPFR_RNDN);     /* 2^32 - 1 */
        emit_case(out, "happy", 0x100000000UL, 53, MPFR_RNDN);   /* 2^32 */
        emit_case(out, "happy", 11UL, 200, MPFR_RNDN);
        emit_case(out, "happy", 13UL, 200, MPFR_RNDN);
        emit_case(out, "happy", 17UL, 200, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50 cases.                                               */
    /* ============================================================== */
    {
        /* (1-5) n=0 at all 5 rnd modes. */
        for (int i = 0; i < 5; ++i) {
            emit_case(out, "edge", 0UL, 53, RNDS[i]);
        }

        /* (6-10) ULONG_MAX at all 5 rnd modes at prec=53 (forces
         * rounding; ULONG_MAX = 2^64 - 1 has 64 bits). */
        for (int i = 0; i < 5; ++i) {
            emit_case(out, "edge", ULONG_MAX, 53, RNDS[i]);
        }

        /* (11-15) ULONG_MAX at prec=64 — exact, since ULONG_MAX is
         * 64 bits all set. */
        for (int i = 0; i < 5; ++i) {
            emit_case(out, "edge", ULONG_MAX, 64, RNDS[i]);
        }

        /* (16-20) ULONG_MAX at prec=63 — forces rounding by 1 bit. */
        for (int i = 0; i < 5; ++i) {
            emit_case(out, "edge", ULONG_MAX, 63, RNDS[i]);
        }

        /* (21-25) ULONG_MAX at prec=1 — extreme rounding. */
        for (int i = 0; i < 5; ++i) {
            emit_case(out, "edge", ULONG_MAX, 1, RNDS[i]);
        }

        /* (26-30) 1 at prec=1 — exact. */
        emit_case(out, "edge", 1UL, 1, MPFR_RNDN);
        emit_case(out, "edge", 1UL, 1, MPFR_RNDZ);
        emit_case(out, "edge", 1UL, 1, MPFR_RNDU);
        emit_case(out, "edge", 1UL, 1, MPFR_RNDD);
        emit_case(out, "edge", 1UL, 1, MPFR_RNDA);

        /* (31-35) Powers of 2 — exact at any prec ≥ 1. */
        emit_case(out, "edge",  1UL << 10, 1, MPFR_RNDN);
        emit_case(out, "edge",  1UL << 30, 1, MPFR_RNDN);
        emit_case(out, "edge",  1UL << 62, 1, MPFR_RNDN);
        emit_case(out, "edge",  1UL << 63, 1, MPFR_RNDN);
        emit_case(out, "edge",  1UL << 32, 5, MPFR_RNDN);

        /* (36-40) 2^k - 1 patterns. */
        emit_case(out, "edge",  (1UL << 10) - 1, 5, MPFR_RNDN);
        emit_case(out, "edge",  (1UL << 20) - 1, 10, MPFR_RNDN);
        emit_case(out, "edge",  (1UL << 30) - 1, 20, MPFR_RNDU);
        emit_case(out, "edge",  (1UL << 50) - 1, 40, MPFR_RNDA);
        emit_case(out, "edge",  (1UL << 63) - 1, 50, MPFR_RNDN);

        /* (41-45) Just past 2^53 (bigger than Number.MAX_SAFE_INTEGER). */
        emit_case(out, "edge",  1UL << 53, 53, MPFR_RNDN);
        emit_case(out, "edge", (1UL << 53) + 1, 53, MPFR_RNDN);
        emit_case(out, "edge", (1UL << 53) + 1, 54, MPFR_RNDN); /* exact at prec 54 */
        emit_case(out, "edge", (1UL << 53) + 7, 53, MPFR_RNDZ);
        emit_case(out, "edge", (1UL << 53) + 7, 53, MPFR_RNDU);

        /* (46-50) Prec edges. We avoid TS_PREC_MAX here because
         * mpfr_init2 at 2^31-257 bits allocates ~256MB and slows the
         * golden generator to a crawl. 4096 bits is well over the
         * practical range. */
        emit_case(out, "edge", 42UL, TS_PREC_MIN, MPFR_RNDN);
        emit_case(out, "edge", 42UL, 4096, MPFR_RNDN);
        emit_case(out, "edge", 3UL, 1, MPFR_RNDN); /* 0b11 → tie → 4 (LSB even on carry) */
        emit_case(out, "edge", 3UL, 1, MPFR_RNDZ); /* 0b11 → trunc → 2 */
        emit_case(out, "edge", 0xCAFEBABEUL, 8, MPFR_RNDN);
    }

    /* ============================================================== */
    /* adversarial: ~30 cases.                                        */
    /* ============================================================== */
    {
        /* Bit-pattern stress at small prec for all 5 rounding modes.
         * Mirror of set_si's pattern set, only positives. */
        const unsigned long patterns[] = {
            0b11011UL,
            0b10101UL,
            0b11111UL,
            0b10001UL,
            0b11100UL,
            0b1101UL,
            0b1011UL,
        };
        const size_t n_pat = sizeof(patterns) / sizeof(patterns[0]);
        for (size_t p = 0; p < n_pat; ++p) {
            for (int r = 0; r < 5; ++r) {
                emit_case(out, "adversarial", patterns[p], 3, RNDS[r]);
            }
        }

        /* (35-) RNDN tie patterns. */
        emit_case(out, "adversarial", 0b1010UL, 2, MPFR_RNDN);  /* tie → even LSB */
        emit_case(out, "adversarial", 0b1110UL, 2, MPFR_RNDN);  /* tie → carry */
    }

    /* ============================================================== */
    /* fuzz: 55 cases.                                                */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x0750D75007550750ULL);
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        for (int rep = 0; rep < 55; ++rep) {
            const unsigned long n = (unsigned long)xs64_next(&rng);
            const uint64_t prec = precs[xs64_below(&rng, 6)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            emit_case(out, "fuzz", n, prec, rnd);
        }
    }

    /* ============================================================== */
    /* mined: 5 cases from mpfr/tests/tset_si.c (the file covers      */
    /* both signed and unsigned).                                     */
    /* ============================================================== */
    {
        emit_case(out, "mined", 0UL, 53, MPFR_RNDN);
        emit_case(out, "mined", 1UL, 53, MPFR_RNDN);
        emit_case(out, "mined", 1024UL, 53, MPFR_RNDN);
        emit_case(out, "mined", ULONG_MAX, 53, MPFR_RNDN);
        emit_case(out, "mined", ULONG_MAX, 64, MPFR_RNDN);
    }

    return 0;
}
