/*
 * golden_driver.c — Golden master for MPFR's mpfr_print_rnd_mode.
 *
 * C signature
 * -----------
 *
 *   const char *mpfr_print_rnd_mode(mpfr_rnd_t rnd_mode);
 *
 *   Source: mpfr/src/print_rnd_mode.c L27-L50. 5-way switch over the
 *   public mpfr_rnd_t enum, returning the canonical "MPFR_RND[NZUDA]"
 *   strings. Returns "MPFR_RNDF" for the (unsupported-in-this-port)
 *   faithful-rounding mode, NULL for any other rnd_mode value.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS port signature: `mpfr_print_rnd_mode(rnd: RoundingMode): {name: string}`.
 *   - Input is the locked-schema string enum 'RNDN' | 'RNDZ' | 'RNDU' |
 *     'RNDD' | 'RNDA' (src/core.ts L137-L151). RNDF is deliberately
 *     unsupported — the TS port throws MPFRError('EROUND') on any other
 *     input value.
 *   - Output is the small record `{name: "MPFR_RND[NZUDA]"}` rather than
 *     a bare string. This is a harness-coupling: the runner's
 *     decodeExpectedOutput (eval/harness/value_codec.ts L274) can't
 *     handle a bare opaque-string scalar, so we wrap.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"rnd":"RND[NZUDA]"},
 *    "output":{"name":"MPFR_RND[NZUDA]"},
 *    "time_ns":<n>}
 *
 *   - `rnd` via jl_kv_rnd (the harness's standard rounding-mode encoder).
 *   - Output wrapped via jl_output_begin_object + jl_kv_str + close,
 *     so the runner's 'object' branch handles the comparison.
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  20  (4× each of the 5 modes — repeated calls)
 *   edge         :  30  (rotations across modes; one-of-each chained)
 *   adversarial  :  10  (each mode appears exactly twice; ordering
 *                        designed so a positional-bias broken port
 *                        loses points)
 *   fuzz         :  50  (PRNG-selected mode for each case)
 *   mined        :   5  (representative call patterns from
 *                        mpfr/tests/tcos.c, tacosu.c, tcan_round.c
 *                        where mpfr_print_rnd_mode is used in error
 *                        messages — the shape is "given a mode, get
 *                        its name", same as every other case).
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/print_rnd_mode.c — C reference.
 * Ref: src/ops/print_rnd_mode.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_print_rnd_mode golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Emit one case. */
static void emit_case(FILE *out, const char *tag, mpfr_rnd_t rnd) {
    const uint64_t t0 = now_ns();
    const char *name = mpfr_print_rnd_mode(rnd);
    const uint64_t elapsed = now_ns() - t0;

    /* The TS port should never be invoked with a mode that returns NULL
     * (the locked-schema RoundingMode forbids it). All 5 supported modes
     * yield a non-NULL string. */
    assert(name != NULL);

    jl_begin(out, tag);
    jl_kv_rnd(out, 1, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_begin_object(out);
    jl_kv_str(out, 1, "name", name);
    jl_output_end_object(out);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* The 5 modes supported by the TS port (src/core.ts L137-L151).
     * Order: alphabetic-by-suffix to match the spec ordering. */
    const mpfr_rnd_t RNDS[5] = {
        MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
    };

    /* ============================================================== */
    /* happy: 20 cases — each mode 4 times.                           */
    /* ============================================================== */
    {
        for (int rep = 0; rep < 4; ++rep) {
            for (int i = 0; i < 5; ++i) {
                emit_case(out, "happy", RNDS[i]);
            }
        }
    }

    /* ============================================================== */
    /* edge: 30 cases — full sweep ordering variants.                 */
    /* ============================================================== */
    {
        /* Forward sweep ×3 = 15. */
        for (int rep = 0; rep < 3; ++rep) {
            for (int i = 0; i < 5; ++i) emit_case(out, "edge", RNDS[i]);
        }
        /* Reverse sweep ×3 = 15. */
        for (int rep = 0; rep < 3; ++rep) {
            for (int i = 4; i >= 0; --i) emit_case(out, "edge", RNDS[i]);
        }
    }

    /* ============================================================== */
    /* adversarial: 10 cases — each mode twice; ordering deliberately */
    /* anti-positional (so a broken port that returns based on index   */
    /* rather than enum dispatch loses).                              */
    /* ============================================================== */
    {
        /* Each mode appears exactly twice; pairs scrambled so neither */
        /* index nor C enum value correlates with order. */
        const mpfr_rnd_t order[10] = {
            MPFR_RNDA, MPFR_RNDN, MPFR_RNDD, MPFR_RNDZ, MPFR_RNDU,
            MPFR_RNDZ, MPFR_RNDA, MPFR_RNDU, MPFR_RNDN, MPFR_RNDD,
        };
        for (int i = 0; i < 10; ++i) {
            emit_case(out, "adversarial", order[i]);
        }
    }

    /* ============================================================== */
    /* fuzz: 50 cases — PRNG-driven mode selection.                  */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x9FAA12345E6F7890ULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t idx = xs64_below(&rng, 5);
            emit_case(out, "fuzz", RNDS[idx]);
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — representative call patterns. Upstream uses   */
    /* mpfr_print_rnd_mode primarily in error-printing code paths;    */
    /* the call always has the form `mpfr_print_rnd_mode(rnd)` where  */
    /* rnd is the active rounding mode. We pick one of each mode to   */
    /* mirror the "any mode could surface in a diagnostic" shape.    */
    /* Examples in mpfr/tests/tcos.c L116, tacosu.c L93, tcan_round.c */
    /* L128.                                                          */
    /* ============================================================== */
    {
        emit_case(out, "mined", MPFR_RNDN);  /* most common in tests */
        emit_case(out, "mined", MPFR_RNDZ);
        emit_case(out, "mined", MPFR_RNDU);
        emit_case(out, "mined", MPFR_RNDD);
        emit_case(out, "mined", MPFR_RNDA);
    }

    return 0;
}
