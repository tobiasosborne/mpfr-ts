/*
 * golden_driver.c -- Golden master for MPFR's mpfr_nbits_uj.
 *
 * C: int mpfr_nbits_uj(uintmax_t n). Ref: mpfr/src/nbits_ulong.c L89-L136.
 *
 * Returns number of significant bits of n, i.e. floor(log2(n)) + 1.
 * Asserts n > 0. On LP64 the result lies in [1, 64].
 *
 * libmpfr-version note: mpfr_nbits_uj is exported by libmpfr (visible
 * via nm -D libmpfr.so | grep mpfr_nbits_uj) but not declared in
 * <mpfr.h> (it lives in mpfr-impl.h, which is not installed). The
 * driver re-declares the prototype below.
 *
 * Wire: {"inputs":{"n":"<u64-dec>"},"output":<int>}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 0
 * (mpfr/tests has no isolatable nbits_uj test -- used internally only).
 */
#include "common.h"
#include <inttypes.h>
#include <stdint.h>

extern int mpfr_nbits_uj(uintmax_t n);

static inline void emit_case(FILE *out, const char *tag, uint64_t n) {
    const uint64_t t0 = now_ns();
    const int result = mpfr_nbits_uj((uintmax_t)n);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "n", n);
    jl_end_inputs(out);
    jl_output_scalar_int(out, result);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- small n in [1, 1000]. */
    {
        const uint64_t happy[20] = {
            1UL, 2UL, 3UL, 4UL, 5UL,
            6UL, 7UL, 8UL, 9UL, 10UL,
            15UL, 16UL, 17UL, 31UL, 32UL,
            100UL, 255UL, 256UL, 500UL, 1000UL,
        };
        for (int i = 0; i < 20; ++i) emit_case(out, "happy", happy[i]);
    }

    /* edge: 30 -- powers of two, pow2-minus-1 across all 64 bit positions. */
    {
        /* Single-bit (powers of two): bits 0..63 (15 reps). */
        emit_case(out, "edge", 1ULL);                       /* bit 0 -> 1 */
        emit_case(out, "edge", 1ULL << 1);                  /* bit 1 -> 2 */
        emit_case(out, "edge", 1ULL << 2);                  /* bit 2 -> 3 */
        emit_case(out, "edge", 1ULL << 3);                  /* bit 3 -> 4 */
        emit_case(out, "edge", 1ULL << 7);                  /* bit 7 -> 8 */
        emit_case(out, "edge", 1ULL << 8);                  /* bit 8 -> 9 */
        emit_case(out, "edge", 1ULL << 15);                 /* bit 15 -> 16 */
        emit_case(out, "edge", 1ULL << 16);                 /* bit 16 -> 17 */
        emit_case(out, "edge", 1ULL << 31);                 /* bit 31 -> 32 */
        emit_case(out, "edge", 1ULL << 32);                 /* bit 32 -> 33 */
        emit_case(out, "edge", 1ULL << 47);                 /* bit 47 -> 48 */
        emit_case(out, "edge", 1ULL << 48);                 /* bit 48 -> 49 */
        emit_case(out, "edge", 1ULL << 62);                 /* bit 62 -> 63 */
        emit_case(out, "edge", 1ULL << 63);                 /* bit 63 -> 64 */
        emit_case(out, "edge", 0xFFFFFFFFFFFFFFFFULL);      /* UINT64_MAX -> 64 */
        /* Powers-of-two minus 1: 2^k - 1 = bits 0..k-1 set (15 reps). */
        emit_case(out, "edge", (1ULL << 1) - 1);            /* 1 -> 1 */
        emit_case(out, "edge", (1ULL << 2) - 1);            /* 3 -> 2 */
        emit_case(out, "edge", (1ULL << 3) - 1);            /* 7 -> 3 */
        emit_case(out, "edge", (1ULL << 4) - 1);            /* 15 -> 4 */
        emit_case(out, "edge", (1ULL << 8) - 1);            /* 255 -> 8 */
        emit_case(out, "edge", (1ULL << 16) - 1);           /* 65535 -> 16 */
        emit_case(out, "edge", (1ULL << 31) - 1);           /* -> 31 */
        emit_case(out, "edge", (1ULL << 32) - 1);           /* -> 32 */
        emit_case(out, "edge", (1ULL << 33) - 1);           /* -> 33 */
        emit_case(out, "edge", (1ULL << 48) - 1);           /* -> 48 */
        emit_case(out, "edge", (1ULL << 49) - 1);           /* -> 49 */
        emit_case(out, "edge", (1ULL << 63) - 1);           /* -> 63 */
        /* Boundary cases at the C divide-and-conquer steps */
        emit_case(out, "edge", 0x10000ULL);                 /* >= 2^16 entry */
        emit_case(out, "edge", 0xFFFFULL);                  /* < 2^16 entry */
        emit_case(out, "edge", 0x100ULL);                   /* >= 2^8 entry */
    }

    /* adversarial: 12 -- u64 max-region, mid-range powers of two, near-flip. */
    {
        emit_case(out, "adversarial", 0xFFFFFFFFFFFFFFFFULL);
        emit_case(out, "adversarial", 0x8000000000000000ULL);
        emit_case(out, "adversarial", 0x8000000000000001ULL);
        emit_case(out, "adversarial", 0x7FFFFFFFFFFFFFFFULL);
        emit_case(out, "adversarial", 0xC000000000000000ULL);
        emit_case(out, "adversarial", 0xAAAAAAAAAAAAAAAAULL);
        emit_case(out, "adversarial", 0x5555555555555555ULL);
        emit_case(out, "adversarial", 0xDEADBEEFDEADBEEFULL);
        emit_case(out, "adversarial", 0xCAFEBABECAFEBABEULL);
        emit_case(out, "adversarial", 0x123456789ABCDEF0ULL);
        emit_case(out, "adversarial", 0x0123456789ABCDEFULL);
        emit_case(out, "adversarial", 0x4ULL);              /* boundary n>=4 branch */
    }

    /* fuzz: 50 random u64 (>= 1; mask off zero just in case). */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x12345678DEADBEEFULL);
        for (int rep = 0; rep < 50; ++rep) {
            uint64_t n = xs64_next(&rng);
            if (n == 0) n = 1;
            emit_case(out, "fuzz", n);
        }
    }

    /* mined: 0 -- no isolated nbits_uj test in mpfr/tests. */
    return 0;
}
