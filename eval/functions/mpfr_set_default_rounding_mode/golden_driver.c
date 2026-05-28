/*
 * golden_driver.c -- Golden master for MPFR's mpfr_set_default_rounding_mode.
 *
 * C signature
 * -----------
 *
 *   void mpfr_set_default_rounding_mode(mpfr_rnd_t rnd_mode);
 *
 *   Body (mpfr/src/set_rnd.c L27-L32):
 *     if (rnd_mode >= MPFR_RNDN && rnd_mode < MPFR_RND_MAX)
 *         __gmpfr_default_rounding_mode = rnd_mode;
 *
 *   Accepts a rounding mode in [MPFR_RNDN, MPFR_RND_MAX) and stores it;
 *   for any value OUTSIDE that range the global is left UNCHANGED (silent
 *   no-op -- no return code, no flag). mpfr_get_default_rounding_mode()
 *   then returns the stored value.
 *
 *   The integer codes (probed from this build's mpfr.h):
 *     0 RNDN, 1 RNDZ, 2 RNDU, 3 RNDD, 4 RNDA, 5 RNDF, RND_MAX == 6,
 *     RNDNA == -1 (retired).
 *   The C guard is `rnd >= MPFR_RNDN(0) && rnd < MPFR_RND_MAX(6)`, so the
 *   ACCEPTED set is the SIX codes 0..5 -- crucially RNDF(5) IS accepted
 *   and stored by the C function (verified empirically: rnd=5 -> output
 *   5, not the prior). REJECTED are rnd < 0 (including the retired
 *   RNDNA=-1) and rnd >= 6, which leave the global at `prior`.
 *
 *   Divergence from the TS five-mode surface: src/core.ts RoundingMode is
 *   the FIVE strings RNDN..RNDA -- RNDF has no string spelling there. This
 *   port therefore operates on the RAW integer code (input + output are
 *   numbers, not RoundingMode strings) and faithfully mirrors the C
 *   accept-range 0..5; the "RNDF is unsupported as a RoundingMode" concern
 *   is deferred to callers that convert a code to a RoundingMode. Keeping
 *   the wire integer is what lets us express the invalid codes (-1, 6, ...)
 *   and the RNDF(5) boundary that a string-only wire could not.
 *
 * Wire shape
 * ----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"rnd":<int>,"prior":<int>},
 *    "output":<int>,
 *    "time_ns":<n>}
 *
 *   - rnd: the requested rounding mode as the raw integer code. Bare JSON
 *     number (jl_kv_int) -> TS number. We use the integer (not the "RNDN"
 *     string) deliberately because the function's domain INCLUDES invalid
 *     codes (-1, 5, ...) that have no string spelling -- the whole point
 *     is to exercise the bounds check.
 *   - prior: the default rounding mode the global is forced to BEFORE the
 *     call (a deterministic baseline). For an out-of-range `rnd` the C
 *     no-op leaves the global at `prior`, so the expected output depends
 *     on it; threading `prior` as an explicit input makes every case
 *     reproducible and order-independent. Bare JSON number -> TS number.
 *   - output: the resulting mpfr_get_default_rounding_mode() as the raw
 *     integer code (0..4). On a valid `rnd` it equals `rnd`; on an
 *     out-of-range `rnd` it equals `prior`. jl_output_scalar_int (bare
 *     JSON number -> TS number).
 *
 * The idiomatic TS port takes (rnd: number, prior: number) and returns a
 * number, mirroring "apply the bounds check; on success the new mode,
 * else the prior mode". The 0..4 <-> RoundingMode string mapping lives in
 * the port; the wire stays integer to keep the invalid-code cases
 * expressible.
 *
 * Driver flow per case (SAVE original / RESTORE around every case):
 *   1. saved = mpfr_get_default_rounding_mode()        -- save live global
 *   2. mpfr_set_default_rounding_mode((mpfr_rnd_t)prior) -- force baseline
 *      (prior is always a valid code, so this always takes effect)
 *   3. mpfr_set_default_rounding_mode((mpfr_rnd_t)rnd)  -- call under test
 *   4. cur = mpfr_get_default_rounding_mode()           -- capture result
 *   5. mpfr_set_default_rounding_mode(saved)            -- restore global
 *   6. emit cur
 * main() restores the start-of-run mode at the end.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 *   happy 20, edge 30, adversarial 12, fuzz 50, mined 5.
 *
 * Compile (standalone; do NOT run build.sh):
 *   gcc -O2 -std=c11 -Wall -Wextra -Werror -I. golden_driver.c \
 *       $(pkg-config --libs mpfr) -lgmp -lm -o golden_driver
 *
 * Ref: mpfr/src/set_rnd.c L27-L39 -- C reference (set/get default rnd).
 * Ref: /usr/include/mpfr.h L100-L111 -- mpfr_rnd_t codes + the
 *   "0=RNDN..4=RNDA" comment; MPFR_RND_MAX sentinel; MPFR_RNDNA=-1 retired.
 * Ref: src/core.ts L137-L151 -- RoundingMode (five modes, no RNDNA/RNDF).
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

/* Emit one case: force `prior`, call with `rnd`, capture, restore. */
static inline void emit_case(FILE *out, const char *tag, int rnd, int prior) {
    /* prior must be a valid code so the baseline always takes effect. */
    assert(prior >= 0 && prior <= 4);
    const mpfr_rnd_t saved = mpfr_get_default_rounding_mode();
    mpfr_set_default_rounding_mode((mpfr_rnd_t)prior);
    const uint64_t t0 = now_ns();
    mpfr_set_default_rounding_mode((mpfr_rnd_t)rnd);
    const uint64_t elapsed = now_ns() - t0;
    const int cur = (int)mpfr_get_default_rounding_mode();
    mpfr_set_default_rounding_mode(saved);  /* restore */

    jl_begin(out, tag);
    jl_kv_int(out, 1, "rnd", rnd);
    jl_kv_int(out, 0, "prior", prior);
    jl_end_inputs(out);
    jl_output_scalar_int(out, cur);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    const mpfr_rnd_t start = mpfr_get_default_rounding_mode();

    /* happy: 20 -- every valid mode set from every (valid) prior, a
     * representative spread (5 modes x 4 distinct priors = 20). */
    emit_case(out, "happy", 0, 0);  /* RNDN from RNDN */
    emit_case(out, "happy", 1, 0);  /* RNDZ from RNDN */
    emit_case(out, "happy", 2, 0);  /* RNDU from RNDN */
    emit_case(out, "happy", 3, 0);  /* RNDD from RNDN */
    emit_case(out, "happy", 4, 0);  /* RNDA from RNDN */
    emit_case(out, "happy", 0, 1);
    emit_case(out, "happy", 1, 1);
    emit_case(out, "happy", 2, 1);
    emit_case(out, "happy", 3, 1);
    emit_case(out, "happy", 4, 1);
    emit_case(out, "happy", 0, 2);
    emit_case(out, "happy", 1, 2);
    emit_case(out, "happy", 2, 2);
    emit_case(out, "happy", 3, 2);
    emit_case(out, "happy", 4, 2);
    emit_case(out, "happy", 0, 4);
    emit_case(out, "happy", 1, 4);
    emit_case(out, "happy", 2, 4);
    emit_case(out, "happy", 3, 4);
    emit_case(out, "happy", 4, 4);

    /* edge: 30 -- valid-from-prior3 set, the accept boundary RNDA(4) and
     * RNDF(5, last accepted), the first REJECTED code 6 (== RND_MAX), and
     * the retired -1 (RNDNA, rejected). */
    emit_case(out, "edge", 0, 3);
    emit_case(out, "edge", 1, 3);
    emit_case(out, "edge", 2, 3);
    emit_case(out, "edge", 3, 3);
    emit_case(out, "edge", 4, 3);   /* RNDA */
    emit_case(out, "edge", 5, 0);   /* RNDF: ACCEPTED (5 < RND_MAX=6) -> 5 */
    emit_case(out, "edge", 5, 1);   /* RNDF accepted -> 5 */
    emit_case(out, "edge", 5, 2);   /* RNDF accepted -> 5 */
    emit_case(out, "edge", 5, 3);   /* RNDF accepted -> 5 */
    emit_case(out, "edge", 5, 4);   /* RNDF accepted -> 5 (last accepted code) */
    emit_case(out, "edge", -1, 0);  /* RNDNA (retired): rejected -> prior 0 */
    emit_case(out, "edge", -1, 1);  /* rejected -> prior 1 */
    emit_case(out, "edge", -1, 2);  /* rejected -> prior 2 */
    emit_case(out, "edge", -1, 3);  /* rejected -> prior 3 */
    emit_case(out, "edge", -1, 4);  /* rejected -> prior 4 */
    emit_case(out, "edge", 6, 0);   /* == RND_MAX: first rejected -> prior 0 */
    emit_case(out, "edge", 7, 1);
    emit_case(out, "edge", 100, 2);
    emit_case(out, "edge", -2, 3);
    emit_case(out, "edge", -100, 4);
    emit_case(out, "edge", 0, 0);   /* identity RNDN->RNDN */
    emit_case(out, "edge", 4, 0);   /* RNDA from RNDN */
    emit_case(out, "edge", 0, 4);   /* RNDN from RNDA */
    emit_case(out, "edge", 2, 3);
    emit_case(out, "edge", 3, 2);
    emit_case(out, "edge", 1, 4);
    emit_case(out, "edge", 4, 1);
    emit_case(out, "edge", 5, 0);   /* re-touch last-accepted code (RNDF) */
    emit_case(out, "edge", 4, 3);   /* re-touch RNDA */
    emit_case(out, "edge", -1, 0);  /* re-touch retired-code boundary */

    /* adversarial: 12 -- the exact accept/reject boundary pair (5 RNDF vs
     * 6 RND_MAX), the retired sentinel (-1), large magnitudes, and
     * prior-preservation checks that catch a port using the wrong
     * comparison (`<=`/`<`) on the RND_MAX bound. */
    emit_case(out, "adversarial", 5, 2);    /* RNDF: last accepted -> 5 */
    emit_case(out, "adversarial", 6, 2);    /* RND_MAX: first rejected -> 2 */
    emit_case(out, "adversarial", -1, 3);   /* retired -> 3 */
    emit_case(out, "adversarial", 0, 4);    /* first accepted (RNDN) */
    emit_case(out, "adversarial", INT32_MAX, 1);   /* huge -> rejected -> 1 */
    emit_case(out, "adversarial", INT32_MIN, 4);   /* huge neg -> rejected -> 4 */
    emit_case(out, "adversarial", 5, 4);    /* RNDF accepted -> 5 */
    emit_case(out, "adversarial", 7, 3);    /* > RND_MAX: rejected -> 3 */
    emit_case(out, "adversarial", -1, 0);
    emit_case(out, "adversarial", 4, 4);    /* RNDA->RNDA idempotent */
    emit_case(out, "adversarial", 0, 0);    /* RNDN->RNDN idempotent */
    emit_case(out, "adversarial", 2, 0);

    /* fuzz: 50 -- PRNG-driven (rnd, prior). rnd spans a band that
     * includes both valid (0..4) and invalid (negative, >=5) codes so
     * roughly half exercise the no-op branch; prior is always valid. */
    {
        xs64_t rng;
        /* Hex seed, digits 0-9 A-F only. */
        xs64_seed(&rng, 0xC0DEFACE5A1AD0EDULL);
        for (int rep = 0; rep < 50; ++rep) {
            /* rnd in [-3, 7] -> mix of valid + rejected. */
            const int rnd = (int)xs64_below(&rng, 11) - 3;
            const int prior = (int)xs64_below(&rng, 5);  /* 0..4, always valid */
            emit_case(out, "fuzz", rnd, prior);
        }
    }

    /* mined: 5 -- the set-default-rounding-mode + restore pattern used in
     * mpfr/tests/tset_rnd-ish loops over RNDN..RNDA, plus a reset-to-RNDN. */
    emit_case(out, "mined", 0, 2);   /* reset to RNDN */
    emit_case(out, "mined", 1, 0);   /* RNDZ */
    emit_case(out, "mined", 2, 0);   /* RNDU */
    emit_case(out, "mined", 3, 0);   /* RNDD */
    emit_case(out, "mined", 4, 0);   /* RNDA */

    mpfr_set_default_rounding_mode(start);  /* restore run start */

    return 0;
}
