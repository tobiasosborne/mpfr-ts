/*
 * golden_driver.c -- Golden master for MPFR's mpfr_round_raw.
 *
 * C signature
 * -----------
 *
 *   int mpfr_round_raw (mp_limb_t       *yp,
 *                       const mp_limb_t *xp,
 *                       mpfr_prec_t      xprec,
 *                       int              neg,
 *                       mpfr_prec_t      yprec,
 *                       mpfr_rnd_t       rnd_mode,
 *                       int             *inexp);
 *
 *   THE core mpn-level rounding primitive. flag=0, use_inexp=1
 *   instantiation of mpfr_round_raw_generic (round_raw_generic.c
 *   L60-L273; instantiated round_prec.c L25-L28). Rounds the
 *   MSB-aligned xprec-bit source mantissa xp (little-endian; xp[0] is
 *   least-significant) to yprec bits in mode rnd_mode for a value of
 *   sign neg (0 positive, 1 negative). Writes ceil(yprec/64) result
 *   limbs into yp, returns the carry (0/1; 1 => power-of-two overflow,
 *   yp wrapped), and writes the inexact flag into *inexp
 *   (in {-2,-1,0,1,2}; +-2 == +-MPFR_EVEN_INEX, the ties-to-even
 *   marker).
 *
 *   mpfr_round_raw is __MPFR_DECLSPEC in mpfr-impl.h, NOT in the public
 *   mpfr.h. Forward-declare for the public-header-only build (same
 *   pattern as eval/functions/mpfr_round_p/golden_driver.c).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"xp":["<dec>",...],     // little-endian limb array
 *              "xprec":"<dec>",        // source precision in bits
 *              "neg":0|1,              // small int (jl_kv_int)
 *              "yprec":"<dec>",        // target precision in bits
 *              "rnd":"RND[NZUDA]"},
 *    "output":{"yp":["<dec>",...],     // ceil(yprec/64) result limbs
 *              "carry":"<dec>",        // 0 or 1 (u64 decimal string)
 *              "inexp":<int>},         // -2..2 (jl_kv_int -> JS number)
 *    "time_ns":<n>}
 *
 *   xp/yp: GMP-limb arrays, decimal-string per limb (lossless BigInt()
 *     on the TS side), little-endian limb order.
 *   xprec/yprec: u64 decimal strings -> bigint (decodeInputValue).
 *   neg: raw JSON int -> JS number 0/1.
 *   carry: u64 decimal string -> bigint (compareField bigint branch).
 *   inexp: raw JSON int -> JS number (compareField number branch,
 *     Object.is, so -2/-1/0/1/2 compared exactly).
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  20  (single-limb xp, xprec>yprec, all 5 rnds,
 *                        clear non-tie rounding)
 *   edge         :  32  (xprec<=yprec no-round copy; carry-overflow;
 *                        neg=0/1 symmetry; multi-limb xp; yprec==1;
 *                        limb-aligned yprec)
 *   adversarial  :  14  (exact halfway ties -> ties-to-even both
 *                        directions; sticky-bit just-above/just-below
 *                        half; carry-on-tie)
 *   fuzz         :  60  (xorshift random limb patterns + random
 *                        xprec/yprec/neg/rnd)
 *   mined        :   5  (round_raw_generic.c documented sub-cases:
 *                        exact, RNDZ trunc, RNDA addone, even-down,
 *                        even-up)
 *
 * Compile (do NOT use build.sh -- races with sibling drivers):
 *   gcc -O2 -std=c11 -Wall -Wextra -Werror -I. \
 *     ../functions/mpfr_round_raw/golden_driver.c \
 *     $(pkg-config --cflags --libs mpfr) -lgmp -lm \
 *     -o ../functions/mpfr_round_raw/golden_driver
 *
 * Ref: mpfr/src/round_raw_generic.c L60-L273 -- the macro body.
 * Ref: mpfr/src/round_prec.c L25-L28         -- the instantiation.
 * Ref: mpfr/src/mpfr-impl.h L1190            -- MPFR_EVEN_INEX == 2.
 * Ref: eval/functions/mpfr_round_p/golden_driver.c -- extern-decl pattern.
 * Ref: eval/functions/mpn_lshift/golden_driver.c   -- limb-array wire.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_round_raw golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* mpfr_round_raw: __MPFR_DECLSPEC in mpfr-impl.h, absent from public
 * mpfr.h. Forward-declare for the public-header-only build. */
extern int mpfr_round_raw (mp_limb_t *, const mp_limb_t *, mpfr_prec_t, int,
                           mpfr_prec_t, mpfr_rnd_t, int *);

#define HIGHBIT  ((mp_limb_t)1 << (GMP_NUMB_BITS - 1))
#define LIMBS_FOR_PREC(p)  ( ((size_t)(p) + GMP_NUMB_BITS - 1) / GMP_NUMB_BITS )

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA
};

/* Emit one mpfr_round_raw golden case.
 *
 * Precondition (round_raw_generic.c L32-L37): xp is non-zero and
 * MSB-aligned -- the MSB of the top limb xp[xn-1] is set, where
 * xn = ceil(xprec/64). We assert it so a malformed driver case is
 * caught at generation time, not silently graded.
 *
 * Timing brackets only the mpfr_round_raw call. */
static inline void emit_case(FILE *out, const char *tag,
                             const mp_limb_t *xp, mpfr_prec_t xprec,
                             int neg, mpfr_prec_t yprec, mpfr_rnd_t rnd) {
    const size_t xn = LIMBS_FOR_PREC(xprec);
    const size_t yn = LIMBS_FOR_PREC(yprec);
    assert(xn >= 1 && yn >= 1);
    assert(xp[xn - 1] & HIGHBIT);     /* MSB-aligned precondition */
    assert(neg == 0 || neg == 1);

    /* yp must hold yn limbs; over-allocate a fixed scratch generously. */
    mp_limb_t *yp = calloc(yn, sizeof(mp_limb_t));
    assert(yp != NULL);
    int inexp = 12345;  /* sentinel: must be overwritten by the call */

    const uint64_t t0 = now_ns();
    const int carry = mpfr_round_raw(yp, xp, xprec, neg, yprec, rnd, &inexp);
    const uint64_t elapsed = now_ns() - t0;

    assert(inexp != 12345);           /* the call always sets *inexp */
    assert(carry == 0 || carry == 1);

    jl_begin(out, tag);
    jl_kv_limbs(out, 1, "xp", xp, xn);
    jl_kv_u64(out, 0, "xprec", (uint64_t)xprec);
    jl_kv_int(out, 0, "neg", neg);
    jl_kv_u64(out, 0, "yprec", (uint64_t)yprec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);

    jl_output_begin_object(out);
    jl_kv_limbs(out, 1, "yp", yp, yn);
    jl_kv_u64(out, 0, "carry", (uint64_t)carry);
    jl_kv_int(out, 0, "inexp", inexp);
    jl_output_end_object(out);
    jl_finish(out, elapsed);

    free(yp);
}

/* Emit the same xp/xprec/yprec across all five rounding modes. */
static inline void emit_all_rnds(FILE *out, const char *tag,
                                 const mp_limb_t *xp, mpfr_prec_t xprec,
                                 int neg, mpfr_prec_t yprec) {
    for (int i = 0; i < 5; ++i)
        emit_case(out, tag, xp, xprec, neg, yprec, RNDS[i]);
}

/* Force the MSB of the top limb of an xn-limb array (MSB-alignment). */
static inline void msb_align(mp_limb_t *xp, size_t xn) {
    xp[xn - 1] |= HIGHBIT;
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 20 -- single-limb xp, xprec=64 > yprec, clear non-ties.  */
    /* ============================================================== */
    {
        /* 0xC000000000000000 = 11000...; round to 2 bits: exact (the
         * dropped bits are all zero). All 5 rnds agree. (5 cases) */
        { mp_limb_t x[1] = { 0xC000000000000000ULL };
          emit_all_rnds(out, "happy", x, 64, 0, 2); }

        /* 0xB000000000000000 = 1011 00..; round to 4 bits drops '...'
         * with the leading dropped bit 0 (below half for the 4-bit
         * target boundary): a non-tie, modes diverge cleanly. (5) */
        { mp_limb_t x[1] = { 0xB400000000000000ULL };
          emit_all_rnds(out, "happy", x, 64, 0, 4); }

        /* A "random looking" mantissa rounded to 10 bits. (5) */
        { mp_limb_t x[1] = { 0x9E3779B97F4A7C15ULL };
          emit_all_rnds(out, "happy", x, 64, 0, 10); }

        /* Negative value (neg=1) rounded to 8 bits. (5) */
        { mp_limb_t x[1] = { 0xDEADBEEF12345678ULL };
          msb_align(x, 1);
          emit_all_rnds(out, "happy", x, 64, 1, 8); }
    }

    /* ============================================================== */
    /* edge: 32 -- no-round copy, carry overflow, alignment, neg sym.  */
    /* ============================================================== */
    {
        /* xprec <= yprec: NO rounding (round_raw_generic.c L108-L123).
         * Copy xp to the top of yp, zero-pad low limbs, inexp=0,
         * carry=0. xprec=2, yprec=64: single small source widened. */
        { mp_limb_t x[1] = { 0x8000000000000000ULL };  /* 1.0 in 2 bits */
          emit_all_rnds(out, "edge", x, 2, 0, 64); }                 /* 5 */
        /* xprec == yprec: also no rounding. */
        { mp_limb_t x[1] = { 0xC000000000000000ULL };
          emit_all_rnds(out, "edge", x, 4, 0, 4); }                  /* 5 */

        /* Carry overflow to power of two: all-ones rounded up. yp wraps
         * to 0 with carry=1 for the away/up directions. (5) */
        { mp_limb_t x[1] = { 0xFFFFFFFFFFFFFFFFULL };
          emit_all_rnds(out, "edge", x, 64, 0, 2); }
        /* Same all-ones, neg=1 (carry behaviour symmetric in magnitude,
         * inexp signs flip). (5) */
        { mp_limb_t x[1] = { 0xFFFFFFFFFFFFFFFFULL };
          emit_all_rnds(out, "edge", x, 64, 1, 2); }

        /* yprec == 1: extreme reduction. round_raw_generic.c L46-L49:
         * 1-bit precision does even-rounding away from 0. (5) */
        { mp_limb_t x[1] = { 0xC000000000000000ULL };
          emit_all_rnds(out, "edge", x, 64, 0, 1); }

        /* Limb-aligned yprec == 64 with xprec == 128 (two-limb source).
         * Top limb significant, low limb dropped. rw == 0 path
         * (round_raw_generic.c L135-L139, lomask=himask=MAX). (5) */
        { mp_limb_t x[2] = { 0x0123456789ABCDEFULL, 0x8000000000000001ULL };
          emit_all_rnds(out, "edge", x, 128, 0, 64); }

        /* Multi-limb source rounded into the second limb (yprec=70:
         * spans two limbs, rw=6). Two cases (neg 0 and 1) at RNDN. */
        { mp_limb_t x[2] = { 0xFEDCBA9876543210ULL, 0xC000000000000000ULL };
          emit_case(out, "edge", x, 128, 0, 70, MPFR_RNDN); }
        { mp_limb_t x[2] = { 0xFEDCBA9876543210ULL, 0xC000000000000000ULL };
          emit_case(out, "edge", x, 128, 1, 70, MPFR_RNDN); }
    }

    /* ============================================================== */
    /* adversarial: 14 -- exact halfway ties + sticky just-off-half.   */
    /* ============================================================== */
    {
        /* Exact half, tie to even DOWN: 0xA000... = 1010 0000...;
         * round to 2 bits. Kept = 10, dropped leading bit = 1 then all
         * zero (exact half). LSB of kept (0) is even -> round down.
         * inexp = -MPFR_EVEN_INEX (-2). neg=0 then neg=1. (2) */
        { mp_limb_t x[1] = { 0xA000000000000000ULL };
          emit_case(out, "adversarial", x, 64, 0, 2, MPFR_RNDN);
          emit_case(out, "adversarial", x, 64, 1, 2, MPFR_RNDN); }

        /* Exact half, tie to even UP: 0xE000... = 1110 0000...; kept =
         * 11 (odd LSB), dropped exact half -> round up to 100, carry=1,
         * yp wraps, inexp = +MPFR_EVEN_INEX (+2). neg 0 and 1. (2) */
        { mp_limb_t x[1] = { 0xE000000000000000ULL };
          emit_case(out, "adversarial", x, 64, 0, 2, MPFR_RNDN);
          emit_case(out, "adversarial", x, 64, 1, 2, MPFR_RNDN); }

        /* Just ABOVE half: 0xA000...0001 (one sticky bit set far down).
         * RNDN must round up (not a tie). All 5 rnds for contrast. (5) */
        { mp_limb_t x[1] = { 0xA000000000000001ULL };
          emit_all_rnds(out, "adversarial", x, 64, 0, 2); }

        /* Just BELOW half: rounding bit 0, sticky set. 0x9000...0001:
         * 1001...; round to 2 bits: rounding bit (bit 61) is 0 ->
         * behaves like RNDZ for RNDN (round down). All 5 rnds. (5) */
        { mp_limb_t x[1] = { 0x9000000000000001ULL };
          emit_all_rnds(out, "adversarial", x, 64, 0, 2); }

        /* Exact-half ties-to-even-DOWN at varied yprec / limb positions.
         * These strengthen the even-rounding-down signal (PIL.3): a
         * mutant that rounds every tie UP must fail these.
         *   - yprec=4: kept 4 bits ...XYZ0, half below, LSB even -> down.
         *     0x9800... = 1001 1000... ; keep 1001 (LSB 1 odd) -> NOT a
         *     down case. Use 0x9000...= 1001 0000.. keep 1001? No.
         *   Construct kept-LSB-even patterns explicitly: keep K bits then
         *   exactly 1000.. (half). For yprec=4 the kept field is bits
         *   63..60; we want bit60 (LSB) == 0 and bit59==1 with the rest 0.
         *     bits: 1 0 0 0 | 1 0...  -> 0x8800000000000000, but MSB must
         *     be set (it is). kept=1000 (LSB 0 even), half below -> DOWN. */
        { mp_limb_t x[1] = { 0x8800000000000000ULL };  /* yprec=4 even-down */
          emit_case(out, "adversarial", x, 64, 0, 4, MPFR_RNDN);
          emit_case(out, "adversarial", x, 64, 1, 4, MPFR_RNDN); }
        /* yprec=4 even-UP counterpart: kept 1001 (LSB odd), half below.
         *   1 0 0 1 | 1 0.. -> 0x9800000000000000. */
        { mp_limb_t x[1] = { 0x9800000000000000ULL };  /* yprec=4 even-up */
          emit_case(out, "adversarial", x, 64, 0, 4, MPFR_RNDN);
          emit_case(out, "adversarial", x, 64, 1, 4, MPFR_RNDN); }
        /* yprec spanning two limbs (yprec=66): kept field LSB in limb 1
         * (the second limb from the bottom). Exact half: bit (128-66-1)=
         * bit 61 of the LOW limb set, all lower zero; kept LSB (bit 62 of
         * low limb... actually bit 0 of the kept field == bit (64-? )).
         * Simpler: two-limb source, top limb 0x8000..0 (kept MSB), low
         * limb 0x4000000000000000 (= exactly half at yprec=66, since
         * 128-66=62 dropped bits and bit 61 is the rounding bit).
         * kept LSB even (low limb's bit 62 == 0) -> DOWN. */
        { mp_limb_t x[2] = { 0x4000000000000000ULL, 0x8000000000000000ULL };
          emit_case(out, "adversarial", x, 128, 0, 66, MPFR_RNDN);
          emit_case(out, "adversarial", x, 128, 1, 66, MPFR_RNDN); }
        /* even-UP across two limbs: set the kept LSB (low limb bit 62)
         * so the tie rounds up. low = 0xC000.. = bit63? No -- need bit62
         * (kept LSB) and bit61 (rounding bit). 0x6000000000000000 sets
         * bits 62 and 61 -> kept LSB odd, exact half -> UP. */
        { mp_limb_t x[2] = { 0x6000000000000000ULL, 0x8000000000000000ULL };
          emit_case(out, "adversarial", x, 128, 0, 66, MPFR_RNDN);
          emit_case(out, "adversarial", x, 128, 1, 66, MPFR_RNDN); }
    }

    /* ============================================================== */
    /* fuzz: 60 -- xorshift random limb patterns + params.            */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x0F1E2D3C4B5A6978ULL);  /* unique hex seed */

        int emitted = 0;
        while (emitted < 60) {
            /* xn in {1,2,3} source limbs. */
            const size_t xn = 1 + (size_t)xs64_below(&rng, 3);
            mp_limb_t xbuf[3];
            for (size_t i = 0; i < xn; ++i)
                xbuf[i] = xs64_next(&rng);
            msb_align(xbuf, xn);  /* enforce MSB-aligned precondition */

            /* xprec: pick a precision whose ceil(/64) == xn so the top
             * limb's MSB is genuinely the value's MSB. Choose xprec in
             * ((xn-1)*64, xn*64]. */
            const uint64_t lo = (uint64_t)(xn - 1) * 64 + 1;
            const uint64_t span = (uint64_t)xn * 64 - lo + 1;
            const mpfr_prec_t xprec =
                (mpfr_prec_t)(lo + xs64_below(&rng, span));

            /* yprec in [1, xprec + 8]: covers xprec>yprec (rounding),
             * xprec==yprec, and xprec<yprec (no-round copy). */
            const mpfr_prec_t yprec =
                (mpfr_prec_t)(1 + xs64_below(&rng, (uint64_t)xprec + 8));

            const int neg = (int)(xs64_next(&rng) & 1);
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            /* If xprec was rounded such that the top MSB isn't actually
             * the precision's MSB, MPFR still treats xp as MSB-aligned
             * to its top limb; our msb_align guarantees the precondition
             * for the limb count, which is what the C asserts. */
            emit_case(out, "fuzz", xbuf, xprec, neg, yprec, rnd);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 -- the documented sub-cases of round_raw_generic.c.    */
    /* ============================================================== */
    {
        /* (1) Exact (round_raw_generic.c L108-L123 / sticky all-zero):
         * 0xC000... to 2 bits, dropped bits zero -> inexp 0, carry 0. */
        { mp_limb_t x[1] = { 0xC000000000000000ULL };
          emit_case(out, "mined", x, 64, 0, 2, MPFR_RNDZ); }
        /* (2) RNDZ truncate with sticky (L204-L220): inexp = +-1. */
        { mp_limb_t x[1] = { 0xC400000000000000ULL };
          emit_case(out, "mined", x, 64, 0, 2, MPFR_RNDZ); }
        /* (3) RNDA add-one-ulp (L221-L255): sticky != 0 -> add ulp,
         * inexp = +-1. */
        { mp_limb_t x[1] = { 0xC400000000000000ULL };
          emit_case(out, "mined", x, 64, 0, 2, MPFR_RNDA); }
        /* (4) Even-rounding DOWN (L156-L172): exact half, kept LSB
         * even -> inexp = -MPFR_EVEN_INEX. */
        { mp_limb_t x[1] = { 0xA000000000000000ULL };
          emit_case(out, "mined", x, 64, 0, 2, MPFR_RNDN); }
        /* (5) Even-rounding UP (L173-L184): exact half, kept LSB odd ->
         * add ulp, inexp = +MPFR_EVEN_INEX. */
        { mp_limb_t x[1] = { 0xE000000000000000ULL };
          emit_case(out, "mined", x, 64, 0, 2, MPFR_RNDN); }
    }

    return 0;
}
