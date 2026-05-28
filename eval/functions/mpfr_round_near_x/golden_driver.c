/*
 * golden_driver.c -- Golden master for MPFR's mpfr_round_near_x.
 *
 * C signature
 * -----------
 *
 *   int mpfr_round_near_x (mpfr_ptr y, mpfr_srcptr v, mpfr_uexp_t err,
 *                          int dir, mpfr_rnd_t rnd);
 *
 *   The "can we round an approximation early?" helper used by the
 *   transcendental functions (the MPFR_FAST_COMPUTE_IF_SMALL_INPUT
 *   wrapper). Assuming y = o(f(x)) = o(x + g(x)) with
 *   |g(x)| < 2^(EXP(v)-err), decides whether v can be rounded to
 *   prec(y) bits to give the correctly-rounded y.
 *     - dir = 0 : error term toward zero (f(x) < x).
 *     - dir = 1 : error term away from zero (f(x) > x).
 *   Returns 0 if it CANNOT round (y left UNMODIFIED); otherwise the
 *   ternary flag (never 0). v must be non-singular.
 *   Ref: mpfr/src/round_near_x.c L26-L45 (contract), L154-L235 (body).
 *
 *   mpfr_round_near_x is __MPFR_DECLSPEC in mpfr-impl.h, not in public
 *   mpfr.h. Forward-declare for the public-header-only build (same
 *   pattern as eval/functions/mpfr_round_p/golden_driver.c).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"v":<MPFR-record>,
 *              "yprec":"<dec>",      // precision of the destination y
 *              "err":"<dec>",        // mpfr_uexp_t error term
 *              "dir":0|1,            // small int (jl_kv_int)
 *              "rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>|null,  // null iff ret==0 (no round)
 *              "ret":<int>},                // 0 (cant round) or +-1
 *    "time_ns":<n>}
 *
 *   value: the rounded y when ret != 0; JSON null when ret == 0 (the
 *     C leaves y unmodified, so the port returns value:null as the
 *     'cannot round' signal). The codec decodes null -> null and
 *     compareField uses strict equality; the port MUST return value:
 *     null (not a stale MPFR) on the cannot-round path.
 *   ret: 0 on cannot-round; +-1 on rounded. Emitted via jl_kv_int ->
 *     JS number; compareField number branch (Object.is).
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  22  (can-round cases: large err, all 5 rnds, both
 *                        dirs, simple v)
 *   edge         :  32  (cannot-round boundary err <= prec(y)+1; err
 *                        just-above threshold; round_p-gated cases;
 *                        yprec extremes; multi-limb v)
 *   adversarial  :  12  (err exactly at the round_p boundary;
 *                        dir/rnd interaction at the exact-set path)
 *   fuzz         :  55  (xorshift random v / err / yprec / dir / rnd)
 *   mined        :   5  (transcendental call-site shapes: err vs
 *                        prec(v) regimes from the contract comment)
 *
 * Compile (do NOT use build.sh -- races with sibling drivers):
 *   gcc -O2 -std=c11 -Wall -Wextra -Werror -I. \
 *     ../functions/mpfr_round_near_x/golden_driver.c \
 *     $(pkg-config --cflags --libs mpfr) -lgmp -lm \
 *     -o ../functions/mpfr_round_near_x/golden_driver
 *
 * Ref: mpfr/src/round_near_x.c L154-L235 -- C reference body.
 * Ref: eval/functions/mpfr_round_p/golden_driver.c -- extern-decl pattern.
 * Ref: src/core.ts -- locked MPFR types (the value field shape).
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_round_near_x golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))

/* mpfr_round_near_x: __MPFR_DECLSPEC in mpfr-impl.h, absent from public
 * mpfr.h. Forward-declare. mpfr_uexp_t is the unsigned exponent type;
 * on this build it is unsigned long (matches mpfr_exp_t width). */
extern int mpfr_round_near_x (mpfr_ptr, mpfr_srcptr, mpfr_uexp_t, int,
                              mpfr_rnd_t);

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA
};

/* Poison value written into y before the call. If ret==0 (cannot
 * round), y is left at this value -- but we emit "value":null in that
 * case, so the poison is never serialised. Choosing a distinctive
 * value makes a driver bug (emitting a stale y) obvious in review. */
#define Y_POISON_SI ((long)-987654321)

/* Emit one mpfr_round_near_x golden case.
 *
 * Precondition (round_near_x.c L164): v must be non-singular. Asserted.
 *
 * Timing brackets only the mpfr_round_near_x call. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr v, uint64_t yprec,
                             unsigned long err, int dir, mpfr_rnd_t rnd) {
    assert(yprec >= 1 && yprec <= TS_PREC_MAX);
    assert(dir == 0 || dir == 1);
    assert(!mpfr_nan_p(v) && !mpfr_inf_p(v) && !mpfr_zero_p(v));

    mpfr_t y;
    mpfr_init2(y, (mpfr_prec_t)yprec);
    mpfr_set_si(y, Y_POISON_SI, MPFR_RNDN);  /* poison to detect no-op */

    const uint64_t t0 = now_ns();
    const int ret = mpfr_round_near_x(y, v, (mpfr_uexp_t)err, dir, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "v", v);
    jl_kv_u64(out, 0, "yprec", yprec);
    jl_kv_u64(out, 0, "err", (uint64_t)err);
    jl_kv_int(out, 0, "dir", dir);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);

    /* Output struct: {value, ret}. value is null on the cannot-round
     * path (ret==0, y unmodified), else the rounded y. We build the
     * object by hand because jl_kv_mpfr can't emit a JSON null. */
    fputs(",\"output\":{", out);
    if (ret == 0) {
        fputs("\"value\":null", out);
    } else {
        jl_kv_mpfr(out, 1, "value", y);
    }
    /* Normalise ret to {-1,0,1}: the rounded paths return the ternary
     * (+-1 in practice; +-2 even-rounding values are folded inside the
     * engine before return, but normalise defensively). */
    const int ret_n = (ret > 0) ? 1 : (ret < 0) ? -1 : 0;
    jl_kv_int(out, 0, "ret", ret_n);
    fputs("}", out);
    jl_finish(out, elapsed);

    mpfr_clear(y);
}

/* ----------------------- helpers ----------------------- */

static inline void init_str_bin(mpfr_ptr x, const char *s, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_str(x, s, 2, MPFR_RNDN);
}
static inline void init_d(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}

/* Emit a value from a binary string across both dirs and all 5 rnds. */
static inline void emit_str_grid(FILE *out, const char *tag, const char *s,
                                 uint64_t vprec, uint64_t yprec,
                                 unsigned long err) {
    for (int dir = 0; dir <= 1; ++dir)
        for (int i = 0; i < 5; ++i) {
            mpfr_t v; init_str_bin(v, s, vprec);
            emit_case(out, tag, v, yprec, err, dir, RNDS[i]);
            mpfr_clear(v);
        }
}

/* ----------------------- main ----------------------- */

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 22 -- can-round cases (large err), both dirs, all rnds.  */
    /* ============================================================== */
    {
        /* v = 1.0 exactly, vprec=60, yprec=10, err=100 (>> prec(y)+1
         * and > prec(v)) -> always roundable. 2 dirs * 5 rnds = 10. */
        emit_str_grid(out, "happy", "1.0", 60, 10, 100);     /* 10 */
        /* v slightly above 1, messy low bits, yprec=20, err=80. 10. */
        emit_str_grid(out, "happy",
                      "1.011010010101110001E0", 60, 20, 80); /* 10 */
        /* A couple of single-mode spot checks at a different magnitude. */
        { mpfr_t v; init_d(v, 3.5, 53); emit_case(out, "happy", v, 12, 90, 1, MPFR_RNDN); mpfr_clear(v); }
        { mpfr_t v; init_d(v, 3.5, 53); emit_case(out, "happy", v, 12, 90, 0, MPFR_RNDZ); mpfr_clear(v); }
    }

    /* ============================================================== */
    /* edge: 32 -- cannot-round boundaries + extremes + multi-limb.   */
    /* ============================================================== */
    {
        /* err <= prec(y)+1 -> cannot round (round_near_x.c L171). With
         * yprec=10, prec(y)+1 = 11, so err in {1,5,10,11} cannot round.
         * y stays at the poison value; we emit value:null. Both dirs,
         * one rnd each to keep the count balanced. */
        { mpfr_t v; init_str_bin(v, "1.0", 60); emit_case(out, "edge", v, 10, 1,  1, MPFR_RNDN); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "1.0", 60); emit_case(out, "edge", v, 10, 5,  0, MPFR_RNDN); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "1.0", 60); emit_case(out, "edge", v, 10, 10, 1, MPFR_RNDZ); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "1.0", 60); emit_case(out, "edge", v, 10, 11, 0, MPFR_RNDA); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "1.0", 60); emit_case(out, "edge", v, 10, 11, 1, MPFR_RNDU); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "1.0", 60); emit_case(out, "edge", v, 10, 11, 0, MPFR_RNDD); mpfr_clear(v); }

        /* err == prec(y)+2 -> just above the first gate. With a clean
         * v=1.0 (exact at any prec) and err > prec(v) is false here
         * (prec(v)=60 >> 12) so the round_p predicate decides. */
        { mpfr_t v; init_str_bin(v, "1.0", 60); emit_case(out, "edge", v, 10, 12, 1, MPFR_RNDN); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "1.0", 60); emit_case(out, "edge", v, 10, 12, 0, MPFR_RNDZ); mpfr_clear(v); }

        /* err > prec(v): the second OR-branch (L172) is satisfied
         * directly, so round_p is bypassed. vprec=10, err=20 > 10. */
        emit_str_grid(out, "edge", "1.1010101010E0", 10, 4, 20); /* 10 */

        /* yprec == 1 extreme: smallest destination. err must exceed
         * prec(y)+1 = 2. Both dirs, all rnds at err=50. */
        emit_str_grid(out, "edge", "1.0", 60, 1, 50);            /* 10 */

        /* Multi-limb v (vprec=130): err between prec(y) and prec(v) so
         * the round_p gate genuinely runs over >1 limb. */
        { mpfr_t v; init_str_bin(v,
            "1.011001110001111100001111100000111110000001111110000000E0", 130);
          emit_case(out, "edge", v, 40, 60, 1, MPFR_RNDN); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v,
            "1.011001110001111100001111100000111110000001111110000000E0", 130);
          emit_case(out, "edge", v, 40, 60, 0, MPFR_RNDZ); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v,
            "1.011001110001111100001111100000111110000001111110000000E0", 130);
          emit_case(out, "edge", v, 40, 200, 1, MPFR_RNDA); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v,
            "1.011001110001111100001111100000111110000001111110000000E0", 130);
          emit_case(out, "edge", v, 40, 200, 0, MPFR_RNDD); mpfr_clear(v); }

        /* Gate-boundary sweep (strengthens mut-coverage of the
         * 'err > prec(y)+1' first gate and the 'prec(y)+(rnd==RNDN)'
         * round_p precision). For each yprec we emit err == yprec+1
         * (MUST be cannot-round: value:null) and err == yprec+2 (passes
         * gate1, round_p then decides). We keep err <= prec(v) so the
         * round_p predicate genuinely runs (not the err>prec(v) bypass).
         * v has a long run of bits so round_p's answer flips with the
         * RNDN +1 in the precision argument. */
        const uint64_t yps[4] = { 5, 12, 20, 33 };
        for (int gi = 0; gi < 4; ++gi) {
            const uint64_t yp = yps[gi];
            /* err = yp+1: first gate fails -> cannot round (null). Test
             * across RNDN and RNDZ so a mutant that weakens the gate
             * produces an observable non-null in BOTH. */
            { mpfr_t v; init_str_bin(v,
                "1.10101010101010101010101010101010101010101010101010101010E0", 100);
              emit_case(out, "edge", v, yp, yp + 1, 1, MPFR_RNDN); mpfr_clear(v); }
            { mpfr_t v; init_str_bin(v,
                "1.10101010101010101010101010101010101010101010101010101010E0", 100);
              emit_case(out, "edge", v, yp, yp + 1, 0, MPFR_RNDZ); mpfr_clear(v); }
            /* err = yp+2: passes gate1; round_p decides. Emit RNDN and a
             * non-RNDN mode so the prec(y)+(rnd==RNDN) distinction is
             * observable (a mutant dropping the +1 changes round_p's
             * answer for RNDN at this boundary). */
            { mpfr_t v; init_str_bin(v,
                "1.10101010101010101010101010101010101010101010101010101010E0", 100);
              emit_case(out, "edge", v, yp, yp + 2, 1, MPFR_RNDN); mpfr_clear(v); }
            { mpfr_t v; init_str_bin(v,
                "1.10101010101010101010101010101010101010101010101010101010E0", 100);
              emit_case(out, "edge", v, yp, yp + 2, 0, MPFR_RNDU); mpfr_clear(v); }

            /* Gate-1-ISOLATING case: err == yp+1 AND err > prec(v), so
             * the round_p inner gate is BYPASSED (round_near_x.c L172
             * second OR is the only inner branch) -- gate-1 alone
             * decides. Correct: cannot round (null). A mutant that
             * weakens gate-1 to 'err > prec(y)' would round here and
             * emit a non-null value, failing the case. Use a tiny
             * prec(v) = yp (so err=yp+1 > prec(v)=yp). v exact (1.0) so
             * the would-be round is well-defined for the mutant. */
            { mpfr_t v; init_str_bin(v, "1.0", yp);
              emit_case(out, "edge", v, yp, yp + 1, 1, MPFR_RNDN); mpfr_clear(v); }
            { mpfr_t v; init_str_bin(v, "1.0", yp);
              emit_case(out, "edge", v, yp, yp + 1, 0, MPFR_RNDZ); mpfr_clear(v); }
        }
    }

    /* ============================================================== */
    /* adversarial: 12 -- err at the round_p boundary; dir/rnd play.   */
    /* ============================================================== */
    {
        /* A v whose mantissa has a run of identical bits in the
         * [prec(y), err] window so mpfr_round_p is on a knife edge.
         * v = 1.0000...0001 (a 1 far down): for moderate err the window
         * is all-zero -> round_p says "cannot round" (ambiguous).
         * Sweep err across the boundary. vprec=80, yprec=20. */
        { mpfr_t v; init_str_bin(v,
            "1.0000000000000000000000000000000000000000000000000000000000000001E0", 80);
          emit_case(out, "adversarial", v, 20, 25, 1, MPFR_RNDN); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v,
            "1.0000000000000000000000000000000000000000000000000000000000000001E0", 80);
          emit_case(out, "adversarial", v, 20, 25, 0, MPFR_RNDN); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v,
            "1.0000000000000000000000000000000000000000000000000000000000000001E0", 80);
          emit_case(out, "adversarial", v, 20, 79, 1, MPFR_RNDN); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v,
            "1.0000000000000000000000000000000000000000000000000000000000000001E0", 80);
          emit_case(out, "adversarial", v, 20, 79, 0, MPFR_RNDZ); mpfr_clear(v); }

        /* A v with all-ones in the window: round_p again ambiguous for
         * small err, decidable for large. */
        { mpfr_t v; init_str_bin(v,
            "1.1111111111111111111111111111111111111111E0", 60);
          emit_case(out, "adversarial", v, 15, 30, 1, MPFR_RNDA); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v,
            "1.1111111111111111111111111111111111111111E0", 60);
          emit_case(out, "adversarial", v, 15, 30, 0, MPFR_RNDZ); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v,
            "1.1111111111111111111111111111111111111111E0", 60);
          emit_case(out, "adversarial", v, 15, 59, 1, MPFR_RNDU); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v,
            "1.1111111111111111111111111111111111111111E0", 60);
          emit_case(out, "adversarial", v, 15, 59, 0, MPFR_RNDD); mpfr_clear(v); }

        /* Negative v: sign-dependent like-RNDZ/like-RNDA branches in the
         * inexact==0 fixup (round_near_x.c L201-L228). */
        { mpfr_t v; init_str_bin(v, "-1.0", 60); emit_case(out, "adversarial", v, 10, 50, 0, MPFR_RNDZ); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "-1.0", 60); emit_case(out, "adversarial", v, 10, 50, 1, MPFR_RNDA); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "-1.0", 60); emit_case(out, "adversarial", v, 10, 50, 0, MPFR_RNDD); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "-1.0", 60); emit_case(out, "adversarial", v, 10, 50, 1, MPFR_RNDU); mpfr_clear(v); }

        /* Gate-1 isolation in the corr-weighted class (adversarial,
         * weight 0.6) so a first-gate off-by-one mutant sinks the
         * composite, not just the 0.2-weight edge class. err == yp+1
         * with err > prec(v) (round_p bypassed) -> gate-1 alone decides
         * -> cannot round (null). A weakened gate-1 ('err > prec(y)')
         * rounds and emits a non-null, failing every case here. */
        {
            const uint64_t yps[6] = { 3, 7, 14, 23, 38, 50 };
            for (int gi = 0; gi < 6; ++gi) {
                const uint64_t yp = yps[gi];
                mpfr_t v; init_str_bin(v, "1.0", yp);  /* prec(v)=yp < err=yp+1 */
                emit_case(out, "adversarial", v, yp, yp + 1,
                          (gi & 1), RNDS[gi % 5]);
                mpfr_clear(v);
            }
        }

        /* round_p RNDN-precision (+1) isolation in adversarial. The
         * key family (verified by direct round_p probing): v = 1.0..01
         * with exactly (yprec-1) zeros before the trailing 1, vprec=80,
         * yprec=10, RNDN. Here round_p(.., prec(y)+1) == 0 (CANNOT round
         * -> correct returns null) while round_p(.., prec(y)) == 1 (the
         * mutant that drops the +1 WOULD round, emitting a non-null).
         * These directly catch a mutant dropping the (rnd==RNDN) +1.
         * The trailing-1 depth is tuned per yprec so the boundary bit
         * lands exactly between prec(y) and prec(y)+1. */
        { mpfr_t v; init_str_bin(v, "1.0000000001E0", 80);            /* yp=10, depth 9 */
          emit_case(out, "adversarial", v, 10, 15, 1, MPFR_RNDN); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "1.0000000001E0", 80);
          emit_case(out, "adversarial", v, 10, 25, 1, MPFR_RNDN); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "1.0000000001E0", 80);
          emit_case(out, "adversarial", v, 10, 40, 0, MPFR_RNDN); mpfr_clear(v); }
        /* yp=6, depth 5: 1.000001 at vprec=80. */
        { mpfr_t v; init_str_bin(v, "1.000001E0", 80);
          emit_case(out, "adversarial", v, 6, 20, 1, MPFR_RNDN); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "1.000001E0", 80);
          emit_case(out, "adversarial", v, 6, 30, 0, MPFR_RNDN); mpfr_clear(v); }
        /* yp=16, depth 15. */
        { mpfr_t v; init_str_bin(v, "1.0000000000000001E0", 80);
          emit_case(out, "adversarial", v, 16, 25, 1, MPFR_RNDN); mpfr_clear(v); }
        { mpfr_t v; init_str_bin(v, "1.0000000000000001E0", 80);
          emit_case(out, "adversarial", v, 16, 45, 0, MPFR_RNDN); mpfr_clear(v); }
        /* Non-RNDN contrast at the same shape: round_p prec is prec(y)
         * for ALL non-RNDN modes, so correct and mutant agree here
         * (these pin that the +1 is RNDN-only). */
        { mpfr_t v; init_str_bin(v, "1.0000000001E0", 80);
          emit_case(out, "adversarial", v, 10, 15, 1, MPFR_RNDZ); mpfr_clear(v); }
    }

    /* ============================================================== */
    /* fuzz: 55 -- xorshift random v / err / yprec / dir / rnd.       */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEE1234567ABDULL);  /* unique hex seed */
        const uint64_t vprecs[5] = { 24, 53, 64, 80, 130 };

        int emitted = 0;
        while (emitted < 55) {
            /* Build a random non-singular v from a random double, then
             * widen to a random vprec. Reject NaN/Inf/zero encodings. */
            const uint64_t bits = xs64_next(&rng);
            const uint64_t exp_bits = (bits >> 52) & 0x7FF;
            if (exp_bits == 0x7FF || exp_bits == 0) continue; /* skip NaN/Inf/sub/zero */
            double d;
            memcpy(&d, &bits, sizeof d);
            if (d == 0.0) continue;

            const uint64_t vprec = vprecs[xs64_below(&rng, 5)];
            mpfr_t v; init_d(v, d, vprec);
            if (mpfr_zero_p(v) || mpfr_nan_p(v) || mpfr_inf_p(v)) { mpfr_clear(v); continue; }

            /* yprec in [1, 50]; err in [1, 2*vprec] so we straddle both
             * the cannot-round and can-round regimes. */
            const uint64_t yprec = 1 + xs64_below(&rng, 50);
            const unsigned long err =
                (unsigned long)(1 + xs64_below(&rng, 2 * vprec));
            const int dir = (int)(xs64_next(&rng) & 1);
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            emit_case(out, "fuzz", v, yprec, err, dir, rnd);
            mpfr_clear(v);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 -- the contract comment's err-vs-prec(x) regimes.     */
    /* (round_near_x.c L47-L152 enumerates the cases; we pick concrete */
    /*  instances of each regime that the transcendentals hit.)       */
    /* ============================================================== */
    {
        /* (1) i==0, dir=ToInf (L53-L62): v exact in y, error away,
         * RNDA adds one ulp. v=1.0 exact at yprec, err large. */
        { mpfr_t v; init_str_bin(v, "1.0", 60); emit_case(out, "mined", v, 10, 100, 1, MPFR_RNDA); mpfr_clear(v); }
        /* (2) i==0, dir=ToZero, RNDZ -> nexttozero (L63-L69). */
        { mpfr_t v; init_str_bin(v, "1.0", 60); emit_case(out, "mined", v, 10, 100, 0, MPFR_RNDZ); mpfr_clear(v); }
        /* (3) i==0, RNDN, either dir -> do nothing (L60,L67). */
        { mpfr_t v; init_str_bin(v, "1.0", 60); emit_case(out, "mined", v, 10, 100, 1, MPFR_RNDN); mpfr_clear(v); }
        /* (4) err > prec(v): the bypass branch (L172 second OR). */
        { mpfr_t v; init_str_bin(v, "1.101E0", 12); emit_case(out, "mined", v, 4, 30, 1, MPFR_RNDN); mpfr_clear(v); }
        /* (5) i != 0 general rounding (L89+): a messy v that does NOT
         * set exactly in y, err in the can-round band. */
        { mpfr_t v; init_str_bin(v, "1.0110100101011100011010E0", 60); emit_case(out, "mined", v, 18, 90, 1, MPFR_RNDN); mpfr_clear(v); }
    }

    return 0;
}
