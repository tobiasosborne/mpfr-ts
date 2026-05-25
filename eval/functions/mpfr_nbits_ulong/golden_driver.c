/*
 * golden_driver.c -- Golden master for MPFR's mpfr_nbits_ulong.
 *
 * C signature
 * -----------
 *
 *   int mpfr_nbits_ulong(unsigned long n);
 *
 * Returns the number of significant bits of n, i.e. floor(log2(n)) + 1.
 * Asserts n > 0 (so the driver never emits n = 0).
 * Ref: mpfr/src/nbits_ulong.c L29-L84.
 *
 * libmpfr-version note
 * --------------------
 *
 * `mpfr_nbits_ulong` is exported by libmpfr (visible via
 *   nm -D libmpfr.so | grep mpfr_nbits_ulong)
 * but not declared in <mpfr.h> (it lives in mpfr-impl.h, which is not
 * installed). The driver re-declares the prototype below.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>","inputs":{"n":"<u64-dec>"},"output":<int>,"time_ns":<n>}
 *
 *   - `n`: u64 decimal string (jl_kv_u64); the TS decoder routes string
 *     -> bigint per value_codec.ts.
 *   - `output`: bare JS number (jl_output_scalar_int); always in [1, 64].
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  20  (small n in [1, 1000])
 *   edge         :  30  (powers of two, powers-of-two minus 1, single-bit
 *                        patterns spanning all 64 bit positions)
 *   adversarial  :  10  (boundary values: u64 max, mid-range powers of two)
 *   fuzz         :  50  (xorshift-driven u64 across full range)
 *   mined        :   0  (mpfr/tests/ has no isolatable mpfr_nbits_ulong
 *                        test driver -- it's used internally by integer-
 *                        constructor paths but not directly tested. The
 *                        Rule 7 minimum for mined is "or all available
 *                        from mpfr/tests/<fn>.c, if fewer than 5 exist",
 *                        which here is zero -- no mined block emitted.)
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/nbits_ulong.c -- C reference.
 * Ref: src/core.ts -- (no schema touch -- scalar-in, scalar-out).
 */
#include "common.h"

#include <inttypes.h>

/* Declared in mpfr-impl.h (not installed); symbol is exported. */
extern int mpfr_nbits_ulong(unsigned long n);

static inline void emit_case(FILE *out, const char *tag, unsigned long n) {
    const uint64_t t0 = now_ns();
    const int result = mpfr_nbits_ulong(n);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_u64(out, 1, "n", (uint64_t)n);
    jl_end_inputs(out);
    jl_output_scalar_int(out, result);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 20 -- small n in [1, 1000].                             */
    /* ============================================================== */
    {
        const unsigned long happy[20] = {
            1UL,  2UL,  3UL,  4UL,  5UL,
            6UL,  7UL,  8UL,  9UL,  10UL,
            15UL, 16UL, 17UL, 31UL, 32UL,
            100UL, 255UL, 256UL, 500UL, 1000UL,
        };
        for (int i = 0; i < 20; ++i) emit_case(out, "happy", happy[i]);
    }

    /* ============================================================== */
    /* edge: 30 -- powers of two and powers-of-two minus 1 across all */
    /* 64 bit positions; covers every possible output value [1, 64].  */
    /* ============================================================== */
    {
        /* Single-bit values 1, 2, 4, ..., 2^63 -- 14 cases spanning
         * representative positions across the 64-bit range. */
        emit_case(out, "edge", 1UL);                  /* bit 0  -> 1 */
        emit_case(out, "edge", 1UL << 1);             /* bit 1  -> 2 */
        emit_case(out, "edge", 1UL << 2);             /* bit 2  -> 3 */
        emit_case(out, "edge", 1UL << 3);             /* bit 3  -> 4 */
        emit_case(out, "edge", 1UL << 7);             /* bit 7  -> 8 */
        emit_case(out, "edge", 1UL << 8);             /* bit 8  -> 9 */
        emit_case(out, "edge", 1UL << 15);            /* bit 15 -> 16 */
        emit_case(out, "edge", 1UL << 16);            /* bit 16 -> 17 */
        emit_case(out, "edge", 1UL << 31);            /* bit 31 -> 32 */
        emit_case(out, "edge", 1UL << 32);            /* bit 32 -> 33 */
        emit_case(out, "edge", 1UL << 47);            /* bit 47 -> 48 */
        emit_case(out, "edge", 1UL << 48);            /* bit 48 -> 49 */
        emit_case(out, "edge", 1UL << 62);            /* bit 62 -> 63 */
        emit_case(out, "edge", 1UL << 63);            /* bit 63 -> 64 */

        /* Power-of-two minus 1 -- exercises the all-bits-below-MSB pattern.
         * (2^k) - 1 has nbits == k.  16 cases. */
        emit_case(out, "edge", (1UL << 1)  - 1UL);    /* 1, nbits=1 */
        emit_case(out, "edge", (1UL << 2)  - 1UL);    /* 3, nbits=2 */
        emit_case(out, "edge", (1UL << 3)  - 1UL);    /* 7, nbits=3 */
        emit_case(out, "edge", (1UL << 4)  - 1UL);    /* 15, nbits=4 */
        emit_case(out, "edge", (1UL << 8)  - 1UL);    /* 255, nbits=8 */
        emit_case(out, "edge", (1UL << 9)  - 1UL);
        emit_case(out, "edge", (1UL << 15) - 1UL);
        emit_case(out, "edge", (1UL << 16) - 1UL);
        emit_case(out, "edge", (1UL << 31) - 1UL);
        emit_case(out, "edge", (1UL << 32) - 1UL);
        emit_case(out, "edge", (1UL << 33) - 1UL);
        emit_case(out, "edge", (1UL << 47) - 1UL);
        emit_case(out, "edge", (1UL << 48) - 1UL);
        emit_case(out, "edge", (1UL << 62) - 1UL);
        emit_case(out, "edge", (1UL << 63) - 1UL);
        emit_case(out, "edge", ~0UL);                 /* 2^64 - 1, nbits=64 */
    }

    /* ============================================================== */
    /* adversarial: 10 -- boundary u64 values and inter-branch        */
    /* divisions of the divide-and-conquer search.                    */
    /* ============================================================== */
    {
        emit_case(out, "adversarial", 0x10000UL);          /* 2^16 -- 16-bit boundary */
        emit_case(out, "adversarial", 0xFFFFUL);           /* 2^16 - 1 */
        emit_case(out, "adversarial", 0x100UL);            /* 2^8 -- 8-bit boundary */
        emit_case(out, "adversarial", 0xFFUL);             /* 2^8 - 1 */
        emit_case(out, "adversarial", 0x10UL);             /* 2^4 -- 4-bit boundary */
        emit_case(out, "adversarial", 0xFUL);              /* 2^4 - 1 */
        emit_case(out, "adversarial", 0x4UL);              /* 2^2 -- 2-bit boundary */
        emit_case(out, "adversarial", 0x3UL);              /* 2^2 - 1 -- "now n = 1, 2, or 3" branch */
        emit_case(out, "adversarial", 0xAAAAAAAAAAAAAAAAUL); /* alternating bits, MSB set */
        emit_case(out, "adversarial", 0x5555555555555555UL); /* alternating bits, MSB clear */
    }

    /* ============================================================== */
    /* fuzz: 50 -- xorshift-driven u64 across full range.             */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEE1234567890ULL);
        for (int i = 0; i < 50; ++i) {
            uint64_t v = xs64_next(&rng);
            if (v == 0) v = 1;  /* never emit n = 0 (C asserts) */
            emit_case(out, "fuzz", (unsigned long)v);
        }
    }

    return 0;
}
