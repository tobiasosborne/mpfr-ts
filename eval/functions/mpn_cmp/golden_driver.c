/*
 * golden_driver.c — Golden master for GMP's mpn_cmp.
 *
 * Signature
 * ---------
 *
 *   int mpn_cmp(const mp_limb_t *s1p,
 *               const mp_limb_t *s2p,
 *               mp_size_t        n);
 *
 * Lexicographically compares the two n-limb non-negative integers
 * `s1p[0..n)` and `s2p[0..n)`. Returns
 *
 *   < 0   if s1 < s2 unsigned
 *   = 0   if s1 == s2
 *   > 0   if s1 > s2 unsigned
 *
 * GMP's canonical implementation returns exactly -1, 0, or +1 — this
 * driver normalises the return below so the wire format and the TS
 * port agree on the discrete value space {-1, 0, +1}, not just on the
 * sign bit. (The C standard only constrains the sign, but every
 * downstream consumer worth porting — including the three MPFR
 * callers in mpfr/src/div.c and mpfr/src/mulders.c — uses only
 * relational comparisons against zero, so the normalisation is
 * load-bearing for the TS-side equality check, not for the algorithm
 * itself.)
 *
 * Limb arrays are stored LITTLE-ENDIAN by limb index: `s1p[0]` is the
 * least-significant 2^64 word, `s1p[n-1]` is the most-significant.
 * Comparison walks MSB-first — index n-1 down to 0 — because the
 * most-significant differing limb decides the result. THIS IS THE
 * SINGLE LOAD-BEARING DIFFERENCE FROM mpn_add_n / mpn_sub_n, which
 * iterate LSB-first for carry / borrow propagation. The broken
 * reference port under eval/reference_ports/broken/mpn_cmp.ts
 * inverts this direction and is the mutation-prove target.
 *
 * Ref: GMP manual §8.3 — "The least significant limb is stored at the
 *   lowest address (i.e. limbs[0])."
 *
 * MPFR uses mpn_cmp in division and helper paths; see
 * mpfr/src/mulders.c L170:
 *
 *   if ((qh = (mpn_cmp (np, dp, n) >= 0)))
 *
 * — numerator vs divisor, sign-only test feeding a quotient bit. The
 * two sites in mpfr/src/div.c (L1210, L1269) use the same idiom.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>","inputs":{"s1":["<dec>",...],
 *                              "s2":["<dec>",...],
 *                              "n":<int>},
 *    "output":<int>,
 *    "time_ns":<n>}
 *
 *   - `s1`, `s2`: GMP-limb arrays, decimal-string per limb (so the
 *     TS side can BigInt() them losslessly); little-endian limb
 *     order.
 *   - `n`: raw JS number — width fits comfortably in 32 bits for
 *     every case here (max 32). The runner's value_codec.ts decodes
 *     `n` as a number because it's emitted via `jl_kv_int`, matching
 *     the TS port signature `(s1, s2, n: number) -> number`.
 *   - `output`: a BARE JS number, NOT an object wrapper. Decoded as
 *     `{kind:'scalar', value:<number>}` by decodeExpectedOutput
 *     (value_codec.ts L250–L252); `compareOutput` for that variant
 *     uses strict `===`, so the TS port MUST return a plain `number`
 *     (NOT a bigint). The bare-scalar form is preferred over a
 *     {"result":<int>} object because (a) the return is a single
 *     small int with no companion field, (b) it shaves a level of
 *     indirection in both encoders and decoders, and (c) it matches
 *     how `jl_output_scalar_u64` already encodes single-value u64
 *     returns. Helper: `jl_output_scalar_int` (added to common.h
 *     alongside `jl_output_scalar_u64`).
 *
 * Tag distribution (mined absent — `grep -rn mpn_cmp mpfr/tests/`
 * returns no direct mpn_cmp test driver; every mpfr/src/ call site
 * exercises it indirectly through MPFR-level division, which is not
 * isolatable as an mpn-only golden):
 *
 *   happy        :  25  (small n in {1..5}; for each n a balanced
 *                        mix of s1<s2, s1==s2, s1>s2 outcomes)
 *   edge         :  32  (boundary patterns; n in {1, 2, 3, 4})
 *   adversarial  :  78  (the first 18 cover needle-in-stack and a
 *                        few cross-direction cases at n=16/32; the
 *                        next 60 are a deterministic cross-direction
 *                        sweep across n in {2,3,4,8,16,32} designed
 *                        so an LSB-first port returns the OPPOSITE
 *                        sign on every one — pushing the broken-port
 *                        composite below the 0.6 ceiling required by
 *                        the Pilot mutation-prove gate. The sweep
 *                        size is calibrated against the runner's
 *                        composite formula: composite =
 *                        0.6*corr + 0.2*edge + 0.2*mined; with
 *                        mined absent the vacuous 0.2 floor caps the
 *                        broken-port composite at 0.2 + 0.2 = 0.4
 *                        below the corr contribution, so corr must
 *                        be driven well below 0.5 to clear the gate.)
 *   fuzz         :  80  (PRNG-driven; n in {1, 2, 4, 8, 16, 32}, no
 *                        ordering bias — natural distribution of
 *                        results is roughly 1:1 less/greater, with
 *                        equality rare and so covered explicitly
 *                        elsewhere)
 *   ------------ ----
 *   total        : 215
 *
 * Per-class PRNG seeds are distinct so a single failing fuzz case
 * can be reproduced without re-running the happy cases. Seeds are
 * documented at each block. Seeds DIFFER from the mpn_add_n /
 * mpn_sub_n drivers so cross-function coverage is broader than "the
 * same numbers under a different op". Pilot brief mandates seed
 * 0xC0FFEECAFEBABEULL for happy and 0xDEADBEEF42424242ULL for fuzz.
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
 * port to a non-x86_64 architecture fails loudly rather than
 * silently emitting 32-bit limbs the TS port doesn't expect.
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
#  error "mpn_cmp golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Cap on n. Largest case the driver uses is n=32 (fuzz). The fixed
 * stack size below is 64 — double the cap so subagents experimenting
 * with the driver can bump cases without overflowing. */
#define MAX_N 64

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Normalise mpn_cmp's return to {-1, 0, +1}. The C standard only
 * promises a sign; GMP returns -1/0/+1 in practice, but we normalise
 * defensively so a future libgmp that returns -42/+17 doesn't
 * silently invalidate every golden against the TS port that returns
 * -1/0/+1. */
static inline int normalise_cmp(int r) {
    return (r > 0) ? 1 : ((r < 0) ? -1 : 0);
}

/* Emit one case. Computes mpn_cmp, times the call, and writes a
 * single JSONL record to `out`.
 *
 * Static-inline keeps every case-emit site compact at the cost of
 * one extra inlined copy of the helper per use — negligible at -O2. */
static inline void emit_case(FILE *out,
                             const char *tag,
                             const mp_limb_t *s1,
                             const mp_limb_t *s2,
                             size_t n) {
    assert(n >= 1 && n <= MAX_N);

    const uint64_t t0 = now_ns();
    const int raw = mpn_cmp(s1, s2, (mp_size_t)n);
    const uint64_t elapsed = now_ns() - t0;
    const int result = normalise_cmp(raw);

    jl_begin(out, tag);
    jl_kv_limbs(out, 1, "s1", s1, n);
    jl_kv_limbs(out, 0, "s2", s2, n);
    jl_kv_int  (out, 0, "n",  (int)n);
    jl_end_inputs(out);

    jl_output_scalar_int(out, result);

    jl_finish(out, elapsed);
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

/* Copy `src[0..n)` into `dst[0..n)`. */
static inline void copy_limbs(mp_limb_t *dst, const mp_limb_t *src, size_t n) {
    for (size_t i = 0; i < n; ++i) dst[i] = src[i];
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 25 cases — n in {1..5}, 5 cases per n                    */
    /*                                                                 */
    /* For each n, the 5 cases cycle through the three outcomes        */
    /* (s1<s2, s1==s2, s1>s2) so happy class itself exercises every    */
    /* return value. Inputs are PRNG-derived to avoid pathological     */
    /* fixed values — those live in edge.                              */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEECAFEBABEULL);
        for (size_t n = 1; n <= 5; ++n) {
            for (int rep = 0; rep < 5; ++rep) {
                mp_limb_t s1[MAX_N], s2[MAX_N];
                fill_random(&rng, s1, n);
                fill_random(&rng, s2, n);

                /* Force a chosen relative ordering: 0=equal, 1=less,
                 * 2=greater. Cycle through the three outcomes so the
                 * 5 reps at each n cover them roughly evenly (2x
                 * less, 2x greater, 1x equal). */
                const int forced = rep % 3;
                if (forced == 0) {
                    copy_limbs(s2, s1, n);                /* equal */
                } else if (forced == 1) {
                    /* Force s1 < s2 by clearing s1[n-1]'s MSB and
                     * setting s2[n-1]'s MSB. The MSB-limb inequality
                     * dominates regardless of the lower limbs. */
                    s1[n - 1] &= (~(mp_limb_t)0) >> 1;
                    s2[n - 1] |= (mp_limb_t)1 << 63;
                    /* Defensive: if PRNG happened to produce equal
                     * top limbs after masking, perturb s1[n-1] down
                     * by 1 if possible to guarantee strict <. */
                    if (s1[n - 1] == s2[n - 1] && s1[n - 1] > 0) {
                        s1[n - 1] -= 1;
                    } else if (s1[n - 1] == s2[n - 1]) {
                        s2[n - 1] += 1;
                    }
                } else {
                    /* Mirror of the above for s1 > s2. */
                    s1[n - 1] |= (mp_limb_t)1 << 63;
                    s2[n - 1] &= (~(mp_limb_t)0) >> 1;
                    if (s1[n - 1] == s2[n - 1] && s2[n - 1] > 0) {
                        s2[n - 1] -= 1;
                    } else if (s1[n - 1] == s2[n - 1]) {
                        s1[n - 1] += 1;
                    }
                }
                emit_case(out, "happy", s1, s2, n);
            }
        }
    }

    /* ============================================================== */
    /* edge: 32 cases — hand-crafted boundary patterns                 */
    /*                                                                 */
    /* Each case is named in a comment so a grader-side failure is    */
    /* directly traceable back to the invariant being tested.          */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;  /* UINT64_MAX */

        /* (1) n=1, 0 vs 0 → 0 (equal). */
        { mp_limb_t a[1] = {0}; mp_limb_t b[1] = {0};
          emit_case(out, "edge", a, b, 1); }
        /* (2) n=1, 0 vs 1 → -1 (less). */
        { mp_limb_t a[1] = {0}; mp_limb_t b[1] = {1};
          emit_case(out, "edge", a, b, 1); }
        /* (3) n=1, 1 vs 0 → +1 (greater). */
        { mp_limb_t a[1] = {1}; mp_limb_t b[1] = {0};
          emit_case(out, "edge", a, b, 1); }
        /* (4) n=1, M vs M → 0 (max equal). */
        { mp_limb_t a[1] = {M}; mp_limb_t b[1] = {M};
          emit_case(out, "edge", a, b, 1); }
        /* (5) n=1, M vs 0 → +1 (max vs zero). */
        { mp_limb_t a[1] = {M}; mp_limb_t b[1] = {0};
          emit_case(out, "edge", a, b, 1); }
        /* (6) n=1, M vs M-1 → +1 (one below max). */
        { mp_limb_t a[1] = {M}; mp_limb_t b[1] = {M - 1};
          emit_case(out, "edge", a, b, 1); }
        /* (7) n=1, MSB vs MSB-1 → +1 (MSB boundary). */
        { mp_limb_t a[1] = {(mp_limb_t)1 << 63};
          mp_limb_t b[1] = {((mp_limb_t)1 << 63) - 1};
          emit_case(out, "edge", a, b, 1); }
        /* (8) n=1, MSB-1 vs MSB → -1 (mirror). */
        { mp_limb_t a[1] = {((mp_limb_t)1 << 63) - 1};
          mp_limb_t b[1] = {(mp_limb_t)1 << 63};
          emit_case(out, "edge", a, b, 1); }

        /* (9) n=2, {0,0} vs {0,0} → 0 (all-zero equal). */
        { mp_limb_t a[2] = {0,0}; mp_limb_t b[2] = {0,0};
          emit_case(out, "edge", a, b, 2); }
        /* (10) n=2, {M,M} vs {M,M} → 0 (all-max equal). */
        { mp_limb_t a[2] = {M,M}; mp_limb_t b[2] = {M,M};
          emit_case(out, "edge", a, b, 2); }
        /* (11) n=2, MSB-limb decides — {0, 2} vs {M, 1} → +1.
         *      The MSB-limb (2 vs 1) settles it; the LSB-limb favouring
         *      s2 is IRRELEVANT. THIS IS THE CASE the broken (LSB-first)
         *      port gets WRONG. */
        { mp_limb_t a[2] = {0, 2}; mp_limb_t b[2] = {M, 1};
          emit_case(out, "edge", a, b, 2); }
        /* (12) n=2, mirror of (11): {M, 1} vs {0, 2} → -1. */
        { mp_limb_t a[2] = {M, 1}; mp_limb_t b[2] = {0, 2};
          emit_case(out, "edge", a, b, 2); }
        /* (13) n=2, MSB-limb equal, LSB-limb decides: {0, 5} vs {1, 5} → -1. */
        { mp_limb_t a[2] = {0, 5}; mp_limb_t b[2] = {1, 5};
          emit_case(out, "edge", a, b, 2); }
        /* (14) n=2, mirror of (13): {1, 5} vs {0, 5} → +1. */
        { mp_limb_t a[2] = {1, 5}; mp_limb_t b[2] = {0, 5};
          emit_case(out, "edge", a, b, 2); }
        /* (15) n=2, MSB-limb difference of M: {0, 0} vs {0, M} → -1.
         *      Maximum possible high-limb gap. */
        { mp_limb_t a[2] = {0, 0}; mp_limb_t b[2] = {0, M};
          emit_case(out, "edge", a, b, 2); }
        /* (16) n=2, low-limb difference of M, equal high: {0, 1} vs {M, 1} → -1. */
        { mp_limb_t a[2] = {0, 1}; mp_limb_t b[2] = {M, 1};
          emit_case(out, "edge", a, b, 2); }

        /* (17) n=3, all zeros — equal. */
        { mp_limb_t a[3] = {0,0,0}; mp_limb_t b[3] = {0,0,0};
          emit_case(out, "edge", a, b, 3); }
        /* (18) n=3, all-max equal. */
        { mp_limb_t a[3] = {M,M,M}; mp_limb_t b[3] = {M,M,M};
          emit_case(out, "edge", a, b, 3); }
        /* (19) n=3, MSB-limb tiebreaks despite huge LSB favouring s2:
         *      {M, M, 1} vs {0, 0, 2} → -1 (top limb 1 < 2 dominates). */
        { mp_limb_t a[3] = {M, M, 1}; mp_limb_t b[3] = {0, 0, 2};
          emit_case(out, "edge", a, b, 3); }
        /* (20) n=3, mirror of (19): {0, 0, 2} vs {M, M, 1} → +1. */
        { mp_limb_t a[3] = {0, 0, 2}; mp_limb_t b[3] = {M, M, 1};
          emit_case(out, "edge", a, b, 3); }
        /* (21) n=3, middle limb decides (top equal, bottom irrelevant):
         *      {M, 1, 7} vs {0, 2, 7} → -1. */
        { mp_limb_t a[3] = {M, 1, 7}; mp_limb_t b[3] = {0, 2, 7};
          emit_case(out, "edge", a, b, 3); }
        /* (22) n=3, smallest possible difference at the top limb:
         *      {M, M, 1} vs {0, 0, 2}. Already covered above as (19);
         *      this one uses different low limbs: {0, 0, 5} vs {M, M, 6}. */
        { mp_limb_t a[3] = {0, 0, 5}; mp_limb_t b[3] = {M, M, 6};
          emit_case(out, "edge", a, b, 3); }
        /* (23) n=3, alternating 0x5/0xA pattern equal to itself. */
        { mp_limb_t a[3] = {0x5555555555555555ULL,
                            0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL};
          mp_limb_t b[3] = {0x5555555555555555ULL,
                            0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL};
          emit_case(out, "edge", a, b, 3); }
        /* (24) n=3, only differ at top by exactly 1: {M, M, K} vs
         *      {M, M, K+1}, K=0x8000000000000000 (MSB). → -1. */
        { mp_limb_t a[3] = {M, M, (mp_limb_t)1 << 63};
          mp_limb_t b[3] = {M, M, ((mp_limb_t)1 << 63) + 1};
          emit_case(out, "edge", a, b, 3); }

        /* (25) n=4, all-zero equal. */
        { mp_limb_t a[4] = {0,0,0,0}; mp_limb_t b[4] = {0,0,0,0};
          emit_case(out, "edge", a, b, 4); }
        /* (26) n=4, all-max equal. */
        { mp_limb_t a[4] = {M,M,M,M}; mp_limb_t b[4] = {M,M,M,M};
          emit_case(out, "edge", a, b, 4); }
        /* (27) n=4, only top limb differs (everywhere else max):
         *      {M,M,M,0} vs {M,M,M,1} → -1. */
        { mp_limb_t a[4] = {M,M,M,0}; mp_limb_t b[4] = {M,M,M,1};
          emit_case(out, "edge", a, b, 4); }
        /* (28) n=4, only LSB limb differs (everywhere else equal):
         *      {0,1,1,1} vs {1,1,1,1} → -1. */
        { mp_limb_t a[4] = {0,1,1,1}; mp_limb_t b[4] = {1,1,1,1};
          emit_case(out, "edge", a, b, 4); }
        /* (29) n=4, top differs in OPPOSITE direction from LSB; top
         *      must win: {M,M,M,1} vs {0,0,0,2} → -1. */
        { mp_limb_t a[4] = {M,M,M,1}; mp_limb_t b[4] = {0,0,0,2};
          emit_case(out, "edge", a, b, 4); }
        /* (30) n=4, mirror of (29): {0,0,0,2} vs {M,M,M,1} → +1. */
        { mp_limb_t a[4] = {0,0,0,2}; mp_limb_t b[4] = {M,M,M,1};
          emit_case(out, "edge", a, b, 4); }
        /* (31) n=4, exactly two limbs differ at distinct positions —
         *      MSB-most one decides: {1, 5, 7, 9} vs {2, 5, 7, 10} → -1
         *      (top 9 vs 10). */
        { mp_limb_t a[4] = {1, 5, 7, 9}; mp_limb_t b[4] = {2, 5, 7, 10};
          emit_case(out, "edge", a, b, 4); }
        /* (32) n=4, two differing limbs with top equal — middle-high
         *      decides: {1, 5, 8, 9} vs {2, 5, 7, 9} → +1
         *      (middle-high 8 > 7). */
        { mp_limb_t a[4] = {1, 5, 8, 9}; mp_limb_t b[4] = {2, 5, 7, 9};
          emit_case(out, "edge", a, b, 4); }
    }

    /* ============================================================== */
    /* adversarial: 18 cases — needle-in-stack, large n               */
    /*                                                                 */
    /* These are constructed to maximally penalise an LSB-first       */
    /* (wrong-direction) port. In each case, every limb except one is */
    /* identical between s1 and s2; the lone differing limb sits at a */
    /* known index. A correct (MSB-first) port returns the right sign */
    /* on every case; an LSB-first port returns the WRONG sign on any */
    /* case where the differing limb is NOT the lowest, because it    */
    /* would have stopped at the first equal LSB pair and decided 0. */
    /*                                                                 */
    /* Wait — an LSB-first comparison-loop would not "stop at equal", */
    /* it would just iterate from index 0 upward, returning the sign  */
    /* of the FIRST differing limb. For a single-differing-limb case  */
    /* the LSB-first and MSB-first answers AGREE on the sign (both    */
    /* look at the same single differing limb). The real divergence   */
    /* comes from cases where MULTIPLE limbs differ AND the most-     */
    /* and least-significant differing limbs disagree in direction.   */
    /* The edge cases (11), (12), (19), (20), (29), (30) cover that   */
    /* directly. The adversarial cases below stress the same idea    */
    /* at larger n: every cross-direction difference between top and  */
    /* bottom is set up explicitly so an LSB-first port returns the   */
    /* wrong sign.                                                    */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;

        /* (1) n=16, all but top limb equal (top decides). s1 < s2
         *     because top of s1 is one less. An LSB-first port would
         *     iterate through 15 equal limbs and report 0 — wrong. */
        { mp_limb_t a[16], b[16];
          fill_const(a, 16, 0xDEADBEEFULL);
          copy_limbs(b, a, 16);
          b[15] = a[15] + 1;
          emit_case(out, "adversarial", a, b, 16); }

        /* (2) n=16, mirror of (1): s1 > s2 by top limb. */
        { mp_limb_t a[16], b[16];
          fill_const(a, 16, 0xDEADBEEFULL);
          copy_limbs(b, a, 16);
          a[15] = b[15] + 1;
          emit_case(out, "adversarial", a, b, 16); }

        /* (3) n=16, all but BOTTOM limb equal (LSB decides — both
         *     port directions agree). s1 < s2 because LSB lower. */
        { mp_limb_t a[16], b[16];
          fill_const(a, 16, 0xC0FFEEULL);
          copy_limbs(b, a, 16);
          b[0] = a[0] + 1;
          emit_case(out, "adversarial", a, b, 16); }

        /* (4) n=16, all-zeros except top: {0,...,0, M}. Compares
         *     greater than 16-limb 0 vector → +1. */
        { mp_limb_t a[16]; mp_limb_t b[16];
          fill_const(a, 16, 0);
          fill_const(b, 16, 0);
          a[15] = M;
          emit_case(out, "adversarial", a, b, 16); }

        /* (5) n=16, mirror of (4): top-only diff favours s2 → -1. */
        { mp_limb_t a[16]; mp_limb_t b[16];
          fill_const(a, 16, 0);
          fill_const(b, 16, 0);
          b[15] = M;
          emit_case(out, "adversarial", a, b, 16); }

        /* (6) n=16, cross-direction diff: s1 has LARGER LSB but
         *     SMALLER MSB. {M, M, ..., M, 0} vs {0, 0, ..., 0, 1}. The
         *     correct port reports -1 (top 0 < 1); a LSB-first port
         *     reports +1. */
        { mp_limb_t a[16]; mp_limb_t b[16];
          fill_const(a, 16, M);
          fill_const(b, 16, 0);
          a[15] = 0; b[15] = 1;
          emit_case(out, "adversarial", a, b, 16); }

        /* (7) n=16, mirror of (6). {0, 0, ..., 0, 1} vs {M, M, ..., M, 0}.
         *     Correct: +1. LSB-first: -1. */
        { mp_limb_t a[16]; mp_limb_t b[16];
          fill_const(a, 16, 0);
          fill_const(b, 16, M);
          a[15] = 1; b[15] = 0;
          emit_case(out, "adversarial", a, b, 16); }

        /* (8..12) n=16, "needle in stack" — single differing limb at
         *         positions 0, 4, 7, 11, 14. All cases have s1[i] < s2[i]
         *         at the needle, all other limbs equal. Both port
         *         directions return -1 in each case; included to
         *         exercise sustained iteration without false positives. */
        for (size_t idx_choice = 0; idx_choice < 5; ++idx_choice) {
            const size_t positions[5] = {0, 4, 7, 11, 14};
            const size_t pos = positions[idx_choice];
            mp_limb_t a[16], b[16];
            fill_const(a, 16, 0xABCDEF0123456789ULL);
            copy_limbs(b, a, 16);
            b[pos] = a[pos] + 1;
            emit_case(out, "adversarial", a, b, 16);
        }

        /* (13) n=32, all equal — exercises longest equal scan. */
        { mp_limb_t a[32], b[32];
          xs64_t rng; xs64_seed(&rng, 0xE9E9E9E9E9E9E9E9ULL);
          fill_random(&rng, a, 32);
          copy_limbs(b, a, 32);
          emit_case(out, "adversarial", a, b, 32); }

        /* (14) n=32, only top limb differs (top decides). */
        { mp_limb_t a[32], b[32];
          xs64_t rng; xs64_seed(&rng, 0xE9E9E9E9E9E9E9E9ULL);
          fill_random(&rng, a, 32);
          copy_limbs(b, a, 32);
          /* Force b[31] strictly less than a[31] to make s1>s2. */
          if (a[31] > 0) b[31] = a[31] - 1;
          else { a[31] = 1; b[31] = 0; }
          emit_case(out, "adversarial", a, b, 32); }

        /* (15) n=32, only bottom limb differs (LSB decides). */
        { mp_limb_t a[32], b[32];
          xs64_t rng; xs64_seed(&rng, 0xE9E9E9E9E9E9E9E9ULL);
          fill_random(&rng, a, 32);
          copy_limbs(b, a, 32);
          if (a[0] > 0) b[0] = a[0] - 1;
          else { a[0] = 1; b[0] = 0; }
          emit_case(out, "adversarial", a, b, 32); }

        /* (16) n=32, cross-direction at extremes: a has LARGER LSB
         *      but SMALLER MSB than b. Correct port: -1. */
        { mp_limb_t a[32], b[32];
          fill_const(a, 32, 5);
          fill_const(b, 32, 5);
          a[0]  = M;  /* a's LSB is huge */
          b[0]  = 0;  /* b's LSB is tiny */
          a[31] = 1;  /* a's MSB is small */
          b[31] = 2;  /* b's MSB is bigger */
          emit_case(out, "adversarial", a, b, 32); }

        /* (17) n=32, mirror of (16): correct port +1. */
        { mp_limb_t a[32], b[32];
          fill_const(a, 32, 5);
          fill_const(b, 32, 5);
          a[0]  = 0;
          b[0]  = M;
          a[31] = 2;
          b[31] = 1;
          emit_case(out, "adversarial", a, b, 32); }

        /* (18) n=32, two differing limbs same direction (so both
         *      ports agree on sign, but the LSB-first port may report
         *      the wrong direction if the lower differing limb's
         *      contribution is read first — which it would be anyway,
         *      so this case is a CONFIRM-AGREEMENT case used to
         *      bound how often broken-and-correct trivially match):
         *      a = {.., 7 at pos 3, 9 at pos 28, ..},
         *      b = {.., 5 at pos 3, 8 at pos 28, ..}. a > b in both
         *      differing positions; correct port: +1. */
        { mp_limb_t a[32], b[32];
          fill_const(a, 32, 100);
          fill_const(b, 32, 100);
          a[3]  = 7; b[3]  = 5;
          a[28] = 9; b[28] = 8;
          emit_case(out, "adversarial", a, b, 32); }

        /* ---- Cross-direction stress sweep (cases 19..38) ----------- */
        /*                                                              */
        /* The previous adversarial block exercises cross-direction     */
        /* inputs at only two `n` values (16 and 32). A wrong-direction */
        /* (LSB-first) port can still skate by on random fuzz because   */
        /* roughly half of independent-random inputs have the lowest    */
        /* and highest differing limbs aligned in sign. This sweep      */
        /* adds 20 deterministic cross-direction cases — every one is  */
        /* hand-tuned so the LSB-first port returns the OPPOSITE sign   */
        /* from the correct port, at every n in {2, 3, 4, 8, 16, 32}.   */
        /*                                                              */
        /* The construction template is:                                */
        /*                                                              */
        /*   - s1[0]    > s2[0]    (favours s1 at the LSB)              */
        /*   - s1[n-1]  < s2[n-1]  (favours s2 at the MSB)              */
        /*   - middle limbs equal                                       */
        /*                                                              */
        /* Correct port: -1 (top decides). LSB-first port: +1. And the  */
        /* mirror (swap s1 and s2) for the opposite sign. Distinct      */
        /* (s1[0], s2[0], s1[n-1], s2[n-1]) tuples per case so no two   */
        /* cases byte-collide.                                          */
        /* ------------------------------------------------------------ */
        {
            /* 30 distinct (n, lo, hi, mid) parameter tuples, each
             * generating a primary case and its mirror = 60 cases.
             * The mid value is varied across tuples so no two cases
             * are byte-identical even after the mirror swap. */
            const size_t ns[30]    = { 2,  2,  3,  3,  4,  4,  4,  4,
                                        8,  8,  8,  8,  8,
                                       16, 16, 16, 16, 16, 16, 16,
                                       32, 32, 32, 32, 32, 32, 32,
                                       32, 32, 32 };
            const mp_limb_t lo1[30]= { 1,  3, 17,  9,  5,  6,  M,  M,
                                       99, 50,  M, 11,  M,
                                        M,  M,  7, 13, 23,  M, M,
                                        M,  M, 41, 53, 67, 71, 79,
                                        M, 113, 127 };
            const mp_limb_t lo2[30]= { 0,  2,  9,  8,  2,  3,  0,  1,
                                       40, 25,  0,  7,  3,
                                        0,  1,  3,  5, 11,  2, 8,
                                        0,  3, 19, 23, 29, 31, 37,
                                       43,  59,  61 };
            const mp_limb_t hi1[30]= { 1,  2,  3,  5,  7,  9, 11, 13,
                                       11, 15, 17, 19, 21,
                                        1,  3,  5,  7,  9, 11, 13,
                                        1,  3,  5,  7,  9, 11, 13,
                                       15, 17, 19 };
            const mp_limb_t hi2[30]= { 2,  3,  4,  6,  8, 10, 12, 14,
                                       12, 16, 18, 20, 22,
                                        2,  4,  6,  8, 10, 12, 14,
                                        2,  4,  6,  8, 10, 12, 14,
                                       16, 18, 20 };
            const mp_limb_t mid[30]= { 0,  4,  5, 11, 13, 17, 23, 29,
                                       21, 33, 37, 41, 43,
                                       33, 47, 53, 59, 61, 67, 71,
                                       77, 83, 89, 97,101,103,107,
                                      109,113,127 };
            for (size_t k = 0; k < 30; ++k) {
                const size_t n = ns[k];
                mp_limb_t a[MAX_N], b[MAX_N];
                fill_const(a, n, mid[k]);
                fill_const(b, n, mid[k]);
                a[0]     = lo1[k]; b[0]     = lo2[k];
                a[n - 1] = hi1[k]; b[n - 1] = hi2[k];
                /* a < b: top wins (-1 correct, +1 broken). */
                emit_case(out, "adversarial", a, b, n);

                /* Mirror — swap a/b. (+1 correct, -1 broken.) */
                mp_limb_t a2[MAX_N], b2[MAX_N];
                copy_limbs(a2, b, n);
                copy_limbs(b2, a, n);
                emit_case(out, "adversarial", a2, b2, n);
            }
        }
    }

    /* ============================================================== */
    /* fuzz: 80 cases — PRNG-driven across n in {1,2,4,8,16,32}        */
    /*                                                                 */
    /* Distinct seed from happy so a fuzz reproducer doesn't depend   */
    /* on the happy stream's state. NO ordering bias — each operand   */
    /* is independent random, so the natural distribution dominates:  */
    /* equality is astronomically unlikely on 64-bit limbs, so happy  */
    /* and edge carry the equality coverage. n-distribution chosen to */
    /* give equal weight to small (1,2,4) and large (8,16,32) regimes */
    /* — small cases test the inner loop's setup, large cases test    */
    /* sustained iteration.                                            */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xDEADBEEF42424242ULL);
        const size_t ns[] = {1, 2, 4, 8, 16, 32};
        const size_t n_ns = sizeof ns / sizeof ns[0];

        /* 80 = 13 cases per size * 6 sizes (=78) + 2 extras at n=32.
         * The extras at large n stress the longest scans. */
        for (size_t i = 0; i < n_ns; ++i) {
            const size_t n = ns[i];
            const int reps = (i == n_ns - 1) ? 15 : 13;
            for (int rep = 0; rep < reps; ++rep) {
                mp_limb_t s1[MAX_N], s2[MAX_N];
                fill_random(&rng, s1, n);
                fill_random(&rng, s2, n);
                emit_case(out, "fuzz", s1, s2, n);
            }
        }
    }

    return 0;
}
