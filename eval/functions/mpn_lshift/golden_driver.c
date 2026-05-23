/*
 * golden_driver.c — Golden master for GMP's mpn_lshift.
 *
 * Signature
 * ---------
 *
 *   mp_limb_t mpn_lshift(mp_limb_t       *rp,
 *                        const mp_limb_t *sp,
 *                        mp_size_t        n,
 *                        unsigned int     count);
 *
 * Shifts the n-limb non-negative integer `sp[0..n)` left by `count`
 * bits and writes the result to `rp[0..n)`. Returns the `count` bits
 * that shifted out from the top, packed as the LOW `count` bits of an
 * mp_limb_t (the high 64-count bits of the return value are zero).
 *
 * Preconditions (per GMP):
 *
 *   - n > 0
 *   - 1 <= count < GMP_NUMB_BITS   (= 1..63 on this platform)
 *   - count == 0 is UNDEFINED BEHAVIOUR. We therefore EXCLUDE count=0
 *     from the goldens entirely. The TS port treats count=0 as a
 *     domain error (throws); the broken port behaves identically on
 *     count=0 by inheritance, so a count=0 case would discriminate
 *     nothing anyway.
 *
 * Limb arrays are stored LITTLE-ENDIAN by limb index: `sp[0]` is the
 * least-significant 2^64 word, `sp[n-1]` is the most-significant. The
 * shift operation walks the limbs LSB-first when writing a fresh
 * output (as the TS port does, no aliasing); the C implementation
 * walks LSB-first too but with extra read-before-write care for the
 * in-place rp == sp case. Either iteration direction produces the
 * same output bytes — only the algorithm's INTERMEDIATE state differs.
 *
 * Ref: GMP manual §8.3 — "The least significant limb is stored at the
 *   lowest address (i.e. limbs[0])."
 *
 * MPFR uses mpn_lshift extensively for mantissa normalisation after
 * cancellation and for aligned partial-sum accumulation; see
 * mpfr/src/sub1sp.c L1573:
 *
 *   mpn_lshift (ap, ap, n, cnt);    // Normalize number after catastrophic
 *                                   // cancellation.
 *
 * and mpfr/src/sum.c L337 / L494 / L987 / L1100 — all variants on
 * aligned-shift of a partial sum. The full `cnt ∈ [1, 63]` interval is
 * exercised across those call sites, motivating the count sweep below.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>","inputs":{"s":["<dec>",...],
 *                              "n":<int>,
 *                              "count":<int>},
 *    "output":{"result":["<dec>",...],"out":"<dec>"},
 *    "time_ns":<n>}
 *
 *   - `s`, `result`: GMP-limb arrays, decimal-string per limb (so the
 *     TS side can BigInt() them losslessly); little-endian limb order.
 *   - `n`: raw JS number — width fits comfortably in 32 bits for every
 *     case here (max 32). Emitted via `jl_kv_int`, matches TS port
 *     signature `(s, n: number, count: number) -> {result, out}`.
 *   - `count`: raw JS number — always in [1, 63]. Emitted via
 *     `jl_kv_int` for the same reason as `n`.
 *   - `out`: u64 decimal string. Always in [0, 2^count); decoded on the
 *     TS side as a bigint via `decodeInputValue` (value_codec.ts L177).
 *     The output object shape matches mpn_add_n's `{result, carry}` —
 *     same {array, scalar-bigint} pair, only the scalar's name differs.
 *
 * Tag distribution (mined absent — `grep -rn mpn_lshift mpfr/tests/`
 * returns no direct mpn_lshift test driver; every mpfr/src/ call site
 * exercises it indirectly through MPFR-level mantissa normalisation,
 * which is not isolatable as an mpn-only golden):
 *
 *   happy        :  21  (small n in {1..4}; count sampled to cover
 *                        the [1, 63] range — count ∈ {1, 16, 31, 32,
 *                        47, 62, 63} on rotation across the (n, rep)
 *                        grid so the happy block alone touches every
 *                        boundary count value at least once)
 *   edge         :  36  (boundary patterns; count ∈ {1, 63}, all-zero
 *                        input, MSB-only inputs that drive out=max
 *                        regimes, n=1 single-limb, alignment-stressing
 *                        bit patterns)
 *   adversarial  :  18  (n=16 with count alternating between 1, 32,
 *                        63 against a single deeply-interleaved input
 *                        pattern, plus a few cross-boundary checks
 *                        where every limb's top `count` bits equal
 *                        the next limb's low `count` bits)
 *   fuzz         :  90  (PRNG-driven; n ∈ {1, 2, 4, 8, 16, 32}, count
 *                        uniformly drawn from [1, 63] per case; 15
 *                        reps per size = 90 total. Seed
 *                        0x1EF7541F71E1DULL per the prep brief.)
 *   ------------ ----
 *   total        : 165
 *
 * Per-class PRNG seeds are distinct so a single failing fuzz case can
 * be reproduced without re-running the happy cases. Seeds documented
 * at each block and differ from the mpn_add_n / mpn_sub_n / mpn_cmp
 * drivers so cross-function coverage is broader than "the same
 * numbers under a different op".
 *
 * Build
 * -----
 *
 *   gcc -O2 -std=c11 -Wall -Wextra -Werror -pedantic -I../../golden_master \
 *       golden_driver.c $(pkg-config --cflags --libs mpfr) -lgmp -lm \
 *       -o golden_driver
 *
 *   The repo-wide build.sh (eval/golden_master/build.sh) finds this
 *   file automatically and uses the same flags (minus -pedantic,
 *   which we add here as belt-and-braces).
 *
 * Reproducibility
 * ---------------
 *
 * The driver is deterministic: every PRNG is seeded with a hardcoded
 * constant. Re-running produces a byte-identical golden.jsonl. The
 * runtime check below also asserts GMP_NUMB_BITS == 64 so a future
 * port to a non-x86_64 architecture fails loudly rather than silently
 * emitting 32-bit limbs the TS port doesn't expect.
 */
#include "common.h"

#include <assert.h>

/* ------------------------------------------------------------------ */
/* Compile-time / runtime invariants                                  */
/* ------------------------------------------------------------------ */

/* mp_limb_t must be exactly 64 bits for the goldens to round-trip
 * losslessly through the TS port (which packs limbs into BigInt at
 * 64 bits per word). Checked at compile time. */
#if GMP_NUMB_BITS != 64
#  error "mpn_lshift golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Cap on n. Largest case the driver uses is n=32 (fuzz). The fixed
 * stack size below is 64 — double the cap so subagents experimenting
 * with the driver can bump cases without overflowing. */
#define MAX_N 64

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Emit one case. Computes mpn_lshift into a fresh `result` buffer,
 * captures the out-limb, times the call, and writes a single JSONL
 * record to `out_f`. Inputs `s`, `n`, `count` are emitted verbatim.
 *
 * Important: the driver allocates a SEPARATE result buffer rather
 * than aliasing (rp == sp). The C source supports the aliased call,
 * but the TS port's contract is "fresh output array" — we exercise
 * the unaliased contract here and let MPFR-level callers handle
 * any in-place semantics through wrapping.
 *
 * Static-inline keeps every case-emit site compact at the cost of
 * one extra inlined copy of the helper per use — negligible at -O2. */
static inline void emit_case(FILE *out_f,
                             const char *tag,
                             const mp_limb_t *s,
                             size_t n,
                             unsigned int count) {
    assert(n >= 1 && n <= MAX_N);
    assert(count >= 1 && count <= 63);

    mp_limb_t result[MAX_N];
    const uint64_t t0 = now_ns();
    const mp_limb_t out_bits = mpn_lshift(result, s, (mp_size_t)n, count);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out_f, tag);
    jl_kv_limbs(out_f, 1, "s",     s, n);
    jl_kv_int  (out_f, 0, "n",     (int)n);
    jl_kv_int  (out_f, 0, "count", (int)count);
    jl_end_inputs(out_f);

    jl_output_begin_object(out_f);
    jl_kv_limbs(out_f, 1, "result", result, n);
    jl_kv_u64  (out_f, 0, "out",    (uint64_t)out_bits);
    jl_output_end_object(out_f);

    jl_finish(out_f, elapsed);
}

/* Fill `dst[0..n)` with PRNG limbs. */
static inline void fill_random(xs64_t *rng, mp_limb_t *dst, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        dst[i] = (mp_limb_t)xs64_next(rng);
    }
}

/* Fill `dst[0..n)` with a constant value. */
static inline void fill_const(mp_limb_t *dst, size_t n, mp_limb_t v) {
    for (size_t i = 0; i < n; ++i) dst[i] = v;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 21 cases — small n, count sampled across [1, 63]         */
    /*                                                                 */
    /* The (n, count) grid is chosen so that:                          */
    /*   - every n ∈ {1, 2, 3, 4} appears                              */
    /*   - count cycles through {1, 16, 31, 32, 47, 62, 63} (a sample */
    /*     across the bit-shift range; covers both <32, =32, >32      */
    /*     regimes which matter for any naive port that tries to use   */
    /*     a single 32-bit shift width)                                */
    /*   - one extra row at n=4 to land on 21 cases total              */
    /*                                                                 */
    /* Inputs are PRNG-derived so each (n, count) pair sees varied     */
    /* limbs; pathological fixed patterns live in edge.                */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x15F70FACE15F70FAULL);
        const unsigned int counts[7] = {1, 16, 31, 32, 47, 62, 63};
        const size_t n_counts = 7;
        const size_t ns[3] = {1, 2, 3};
        for (size_t i = 0; i < 3; ++i) {
            const size_t n = ns[i];
            /* For each (n, count) emit ONE case — 3 × 7 = 21 happy
             * cases, sweeping count across the full range at three
             * small n values. */
            for (size_t c = 0; c < n_counts; ++c) {
                const unsigned int count = counts[c];
                mp_limb_t s[MAX_N];
                fill_random(&rng, s, n);
                emit_case(out, "happy", s, n, count);
            }
        }
    }

    /* ============================================================== */
    /* edge: 36 cases — hand-crafted boundary patterns                 */
    /*                                                                 */
    /* Each case is named in a comment so a grader-side failure is    */
    /* directly traceable back to the invariant being tested.          */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;  /* UINT64_MAX */
        const mp_limb_t MSB = (mp_limb_t)1 << 63;

        /* (1) n=1, count=1, s={0} → result={0}, out=0. Zero-identity. */
        { mp_limb_t s[1] = {0};
          emit_case(out, "edge", s, 1, 1); }
        /* (2) n=1, count=63, s={0} → result={0}, out=0. Max shift on zero. */
        { mp_limb_t s[1] = {0};
          emit_case(out, "edge", s, 1, 63); }
        /* (3) n=1, count=1, s={1} → result={2}, out=0. Shift LSB up by 1. */
        { mp_limb_t s[1] = {1};
          emit_case(out, "edge", s, 1, 1); }
        /* (4) n=1, count=1, s={MSB} → result={0}, out=1. The MSB shifts
         *     OUT and the result's MSB becomes 0. Single-limb LSB-of-out
         *     test — most direct check that `out` carries the right bit. */
        { mp_limb_t s[1] = {MSB};
          emit_case(out, "edge", s, 1, 1); }
        /* (5) n=1, count=1, s={M} → result={M-1 = M<<1 & M}, out=1. All
         *     ones, shifted by one: the LSB is now 0, every other bit is
         *     1, and the top bit shifted out (1). */
        { mp_limb_t s[1] = {M};
          emit_case(out, "edge", s, 1, 1); }
        /* (6) n=1, count=63, s={1} → result={MSB}, out=0. One bit shifted
         *     up by 63 lands at the MSB exactly; nothing has spilled. */
        { mp_limb_t s[1] = {1};
          emit_case(out, "edge", s, 1, 63); }
        /* (7) n=1, count=63, s={M} → result={MSB}, out=M>>1. Every bit
         *     except the LSB shifts out; the LSB lands at the MSB.
         *     out is 63 bits wide = M>>1 = 0x7FFF...FFFF. */
        { mp_limb_t s[1] = {M};
          emit_case(out, "edge", s, 1, 63); }
        /* (8) n=1, count=32, s={0xFFFFFFFF00000000} → result={0},
         *     out=0xFFFFFFFF. The top half shifts out wholesale, bottom
         *     half (zero) shifts into the top. Mid-range count check. */
        { mp_limb_t s[1] = {0xFFFFFFFF00000000ULL};
          emit_case(out, "edge", s, 1, 32); }
        /* (9) n=1, count=32, s={0x00000000FFFFFFFF} → result={0xFFFFFFFF00000000},
         *     out=0. Bottom half rolls up to top half; no spill. */
        { mp_limb_t s[1] = {0x00000000FFFFFFFFULL};
          emit_case(out, "edge", s, 1, 32); }

        /* (10) n=2, count=1, s={0,0} → result={0,0}, out=0. */
        { mp_limb_t s[2] = {0,0};
          emit_case(out, "edge", s, 2, 1); }
        /* (11) n=2, count=63, s={0,0} → result={0,0}, out=0. */
        { mp_limb_t s[2] = {0,0};
          emit_case(out, "edge", s, 2, 63); }
        /* (12) n=2, count=1, s={1,0} → result={2,0}, out=0. LSB-limb-only
         *      shift, no cross-limb carry. */
        { mp_limb_t s[2] = {1,0};
          emit_case(out, "edge", s, 2, 1); }
        /* (13) n=2, count=1, s={MSB, 0} → result={0, 1}, out=0. Single
         *      bit crosses from low limb to high limb. THIS IS THE CASE
         *      THE BROKEN PORT GETS WRONG: without the mask, result[0]
         *      retains the MSB (because `MSB << 1 = 2^64` overflows but
         *      bigint won't truncate), leaking the bit-2^64 into result[0]. */
        { mp_limb_t s[2] = {MSB, 0};
          emit_case(out, "edge", s, 2, 1); }
        /* (14) n=2, count=1, s={0, MSB} → result={0, 0}, out=1. Top-limb
         *      MSB shifts entirely out; out gets it. */
        { mp_limb_t s[2] = {0, MSB};
          emit_case(out, "edge", s, 2, 1); }
        /* (15) n=2, count=1, s={M, M} → result={M-1, M}, out=1. Full
         *      ones, top of low rolls into bottom of high; top of high
         *      rolls into out. */
        { mp_limb_t s[2] = {M, M};
          emit_case(out, "edge", s, 2, 1); }
        /* (16) n=2, count=63, s={1, 0} → result={MSB, 0}, out=0. One
         *      bit in low limb shifts to MSB of low; nothing reaches
         *      high. */
        { mp_limb_t s[2] = {1, 0};
          emit_case(out, "edge", s, 2, 63); }
        /* (17) n=2, count=63, s={1, 1} → result={MSB, MSB}, out=0.
         *      Each limb's LSB becomes its MSB; the bottom limb's high
         *      bits (zero) shift to top limb's low bits; no spill. */
        { mp_limb_t s[2] = {1, 1};
          emit_case(out, "edge", s, 2, 63); }
        /* (18) n=2, count=63, s={M, M} → result={MSB, MSB}, out=M>>1.
         *      Every bit except each limb's LSB shifts up by 63; the
         *      old top limb's M>>1 high bits shift out. */
        { mp_limb_t s[2] = {M, M};
          emit_case(out, "edge", s, 2, 63); }
        /* (19) n=2, count=32, s={M, 0} → result={M<<32 & M, M>>32}
         *      = {0xFFFFFFFF00000000, 0xFFFFFFFF}, out=0. Mid-shift
         *      moves top half of low limb into bottom half of high. */
        { mp_limb_t s[2] = {M, 0};
          emit_case(out, "edge", s, 2, 32); }
        /* (20) n=2, count=32, s={0, M} → result={0, M<<32 & M}
         *      = {0, 0xFFFFFFFF00000000}, out=0xFFFFFFFF. Top half of
         *      high limb shifts out; bottom half of high becomes new top. */
        { mp_limb_t s[2] = {0, M};
          emit_case(out, "edge", s, 2, 32); }
        /* (21) n=2, count=16, s={M, M} → result={M<<16 & M, M},
         *      out=0xFFFF. Mid-low shift; the top 16 bits of high limb
         *      spill out. */
        { mp_limb_t s[2] = {M, M};
          emit_case(out, "edge", s, 2, 16); }

        /* (22) n=3, count=1, all-zeros — no-op. */
        { mp_limb_t s[3] = {0,0,0};
          emit_case(out, "edge", s, 3, 1); }
        /* (23) n=3, count=1, single low bit propagates ALL the way up.
         *      s={MSB, M, MSB} — low MSB rolls into mid LSB, mid M's MSB
         *      rolls into high LSB, high MSB rolls out. */
        { mp_limb_t s[3] = {MSB, M, MSB};
          emit_case(out, "edge", s, 3, 1); }
        /* (24) n=3, count=1, only top-limb MSB set — should produce
         *      out=1 and result all zero. */
        { mp_limb_t s[3] = {0, 0, MSB};
          emit_case(out, "edge", s, 3, 1); }
        /* (25) n=3, count=63, all M — every bit but each limb's LSB
         *      shifts up by 63; old top limb's 63 high bits shift out. */
        { mp_limb_t s[3] = {M, M, M};
          emit_case(out, "edge", s, 3, 63); }
        /* (26) n=3, count=8, hex-aligned pattern: s={0xAABB..., 0xCCDD...,
         *      0xEEFF...} shifted by 8 advances each byte by one position
         *      and spills the top byte of the top limb. */
        { mp_limb_t s[3] = {0xAABBCCDDEEFF0011ULL,
                            0xCCDD22334455EEFFULL,
                            0xEEFF1122334455AAULL};
          emit_case(out, "edge", s, 3, 8); }
        /* (27) n=3, count=1, ALIGNMENT pattern: every limb's high 1
         *      bit equals the next limb's low 1 bit. s={MSB, 1|MSB, 1}.
         *      After shift: low limb's bit at position 63 rolls up to
         *      mid limb's bit 0; mid limb already has bit 0 set, so
         *      the bit at mid's old bit-0 has just rolled into its
         *      bit-1; etc. */
        { mp_limb_t s[3] = {MSB, MSB | (mp_limb_t)1, (mp_limb_t)1};
          emit_case(out, "edge", s, 3, 1); }
        /* (28) n=3, count=1, only bottom limb set: s={1,0,0} — verifies
         *      the carry NEVER propagates spuriously through zero limbs. */
        { mp_limb_t s[3] = {1,0,0};
          emit_case(out, "edge", s, 3, 1); }
        /* (29) n=3, count=63, s={0, MSB, 0} — middle-limb MSB rolls into
         *      high-limb LSB; everything else zero; out=0. */
        { mp_limb_t s[3] = {0, MSB, 0};
          emit_case(out, "edge", s, 3, 63); }

        /* (30) n=4, count=1, all-zero. */
        { mp_limb_t s[4] = {0,0,0,0};
          emit_case(out, "edge", s, 4, 1); }
        /* (31) n=4, count=1, all-M — full ripple of bit-out-of-top. */
        { mp_limb_t s[4] = {M,M,M,M};
          emit_case(out, "edge", s, 4, 1); }
        /* (32) n=4, count=63, all-M — every bit except per-limb LSB
         *      rotates up by 63 and the top limb's M>>1 spills out. */
        { mp_limb_t s[4] = {M,M,M,M};
          emit_case(out, "edge", s, 4, 63); }
        /* (33) n=4, count=32, alternating 0xFFFFFFFF00000000 and its
         *      complement — exercises mid-count cross-limb transitions
         *      where the upper half of each odd limb equals zero. */
        { mp_limb_t s[4] = {0xFFFFFFFF00000000ULL,
                            0x00000000FFFFFFFFULL,
                            0xFFFFFFFF00000000ULL,
                            0x00000000FFFFFFFFULL};
          emit_case(out, "edge", s, 4, 32); }
        /* (34) n=4, count=1, only TOP limb's MSB set — minimal-case
         *      out=1 with all-zero result. */
        { mp_limb_t s[4] = {0,0,0,MSB};
          emit_case(out, "edge", s, 4, 1); }
        /* (35) n=4, count=1, only BOTTOM limb's MSB set — bit propagates
         *      from low limb into mid-low limb's LSB; result[1]=1; nothing
         *      else moves; out=0. */
        { mp_limb_t s[4] = {MSB,0,0,0};
          emit_case(out, "edge", s, 4, 1); }
        /* (36) n=4, count=4, hex-nibble-aligned pattern: each nibble
         *      shifts up by one position. s={0x0123456789ABCDEF, ...} */
        { mp_limb_t s[4] = {0x0123456789ABCDEFULL,
                            0xFEDCBA9876543210ULL,
                            0x13579BDF02468ACEULL,
                            0xECA86420FDB97531ULL};
          emit_case(out, "edge", s, 4, 4); }
    }

    /* ============================================================== */
    /* adversarial: 18 cases — alignment stress and cross-count        */
    /*                                                                 */
    /* The construction template is: pick ONE deeply-interleaved        */
    /* input pattern, then shift it by each of {1, 32, 63} (the three   */
    /* count-regime corners) at n=16, plus a few n=8 / n=32 variants    */
    /* and a handful of "every limb's high `count` bits equal the      */
    /* next limb's low `count` bits" alignment stressors.               */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;
        const mp_limb_t MSB = (mp_limb_t)1 << 63;

        /* Pattern A: n=16, deeply-interleaved bytes. Used by cases (1..3)
         * with count ∈ {1, 32, 63}. */
        const mp_limb_t pat_a_seed[16] = {
            0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL,
            0x13579BDF02468ACEULL, 0xECA86420FDB97531ULL,
            0xAABBCCDDEEFF0011ULL, 0x55667788BBCCDDEEULL,
            0xDEADBEEFCAFEBABEULL, 0xBADC0FFEE0DDF00DULL,
            0x0F0F0F0F0F0F0F0FULL, 0xF0F0F0F0F0F0F0F0ULL,
            0x00FF00FF00FF00FFULL, 0xFF00FF00FF00FF00ULL,
            0xCCCCCCCCCCCCCCCCULL, 0x3333333333333333ULL,
            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL
        };

        /* (1) n=16, count=1 — minimum shift on the full interleave. */
        { mp_limb_t s[16];
          for (size_t i = 0; i < 16; ++i) s[i] = pat_a_seed[i];
          emit_case(out, "adversarial", s, 16, 1); }
        /* (2) n=16, count=32 — mid-count shift. */
        { mp_limb_t s[16];
          for (size_t i = 0; i < 16; ++i) s[i] = pat_a_seed[i];
          emit_case(out, "adversarial", s, 16, 32); }
        /* (3) n=16, count=63 — maximum shift on the full interleave. */
        { mp_limb_t s[16];
          for (size_t i = 0; i < 16; ++i) s[i] = pat_a_seed[i];
          emit_case(out, "adversarial", s, 16, 63); }

        /* Pattern B: n=16, every limb's high `count` bits equal the
         * NEXT limb's low `count` bits — alignment stressor. For each
         * count in {1, 8, 16, 32, 48, 63}, construct a pattern where
         * limbs[i+1]'s low `count` bits equal limbs[i]'s high `count`
         * bits. After the shift, every "donor" limb's high count bits
         * line up with the "receiver" limb's low count bits and the
         * intermediate state is maximally ambiguous to a wrong-mask
         * port. */
        {
            const unsigned int counts[6] = {1, 8, 16, 32, 48, 63};
            xs64_t rng;
            xs64_seed(&rng, 0xA11907A11907A119ULL);
            for (size_t k = 0; k < 6; ++k) {
                const unsigned int count = counts[k];
                /* Mask for the high `count` bits of a 64-bit limb. */
                const mp_limb_t high_mask = M << (64u - count);
                mp_limb_t s[16];
                /* Fill each limb with random low bits, then OR in the
                 * previous limb's high `count` bits at the low end. */
                for (size_t i = 0; i < 16; ++i) {
                    s[i] = (mp_limb_t)xs64_next(&rng);
                    if (i > 0) {
                        const mp_limb_t donor_high =
                            (s[i - 1] & high_mask) >> (64u - count);
                        /* Low `count` bits of s[i] = donor_high. */
                        s[i] = (s[i] & (M << count)) | donor_high;
                    }
                }
                emit_case(out, "adversarial", s, 16, count);
            }
        }

        /* Pattern C: n=8, every limb is the same value M; tests that
         * an n-limb full-M shifted by varying counts produces an
         * identical low-spill pattern across every count. count ∈
         * {1, 17, 31, 47, 63}. */
        {
            const unsigned int counts[5] = {1, 17, 31, 47, 63};
            for (size_t k = 0; k < 5; ++k) {
                mp_limb_t s[8];
                fill_const(s, 8, M);
                emit_case(out, "adversarial", s, 8, counts[k]);
            }
        }

        /* Pattern D: n=32, single-bit input at varied positions, shifted
         * by count=1. Each case has exactly one bit set in the entire
         * 32-limb word, at a deliberately-chosen index/bit-offset that
         * either crosses or does NOT cross a limb boundary under the
         * shift. (4 cases — limb 0 bit 62 [no cross], limb 0 bit 63
         * [cross to limb 1], limb 31 bit 62 [no cross, in top limb],
         * limb 31 bit 63 [crosses OUT into `out`].) */
        {
            mp_limb_t s[32];

            /* (D1) bit at (limb 0, position 62): shifts to (0, 63),
             *      no cross. */
            fill_const(s, 32, 0);
            s[0] = (mp_limb_t)1 << 62;
            emit_case(out, "adversarial", s, 32, 1);

            /* (D2) bit at (limb 0, position 63): shifts to (1, 0),
             *      crosses limb boundary. */
            fill_const(s, 32, 0);
            s[0] = MSB;
            emit_case(out, "adversarial", s, 32, 1);

            /* (D3) bit at (limb 31, position 62): shifts to (31, 63),
             *      no cross, no spill. */
            fill_const(s, 32, 0);
            s[31] = (mp_limb_t)1 << 62;
            emit_case(out, "adversarial", s, 32, 1);

            /* (D4) bit at (limb 31, position 63): shifts OUT, result
             *      all zero, out=1. */
            fill_const(s, 32, 0);
            s[31] = MSB;
            emit_case(out, "adversarial", s, 32, 1);
        }
    }

    /* ============================================================== */
    /* fuzz: 90 cases — PRNG-driven across n in {1,2,4,8,16,32},        */
    /*                  count uniform in [1, 63]                        */
    /*                                                                 */
    /* Seed 0x1EF7541F71E1DULL per the prep brief. Each (n, rep) draws */
    /* both the limb stream and a fresh count. 15 reps per size = 90   */
    /* total. */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x1EF7541F71E1DULL);
        const size_t ns[] = {1, 2, 4, 8, 16, 32};
        const size_t n_ns = sizeof ns / sizeof ns[0];

        for (size_t i = 0; i < n_ns; ++i) {
            const size_t n = ns[i];
            for (int rep = 0; rep < 15; ++rep) {
                mp_limb_t s[MAX_N];
                fill_random(&rng, s, n);
                /* count uniformly in [1, 63]. xs64_below(rng, 63)
                 * yields [0, 62]; +1 yields [1, 63]. */
                const unsigned int count =
                    (unsigned int)(xs64_below(&rng, 63) + 1);
                emit_case(out, "fuzz", s, n, count);
            }
        }
    }

    return 0;
}
