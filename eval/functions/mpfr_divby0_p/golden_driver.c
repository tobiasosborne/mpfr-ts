/*
 * golden_driver.c -- Golden master for MPFR's mpfr_divby0_p.
 *
 * The C function reads __gmpfr_flags & MPFR_FLAGS_DIVBY0 (bit 0x20)
 * and returns 0 or non-zero. The TS port returns boolean per the
 * idiomatic-TS lift (ADR 0001).
 *
 * Because the predicate is stateless on its surface but state-dependent
 * in reality (it reads a global flag register), each case takes a
 * `mask: bigint` input describing the desired pre-test flag state. The
 * driver:
 *   1. mpfr_clear_flags()
 *   2. mpfr_flags_set(mask)
 *   3. read mpfr_divby0_p()
 *
 * The TS port must perform the equivalent composition against a TS-side
 * flag register (currently an API gap -- see spec.json divergence_from_c).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"mask":"<dec>"},
 *    "output":true|false,
 *    "time_ns":<n>}
 *
 *   - mask is a uint64 emitted via jl_kv_u64 (decimal string, BigInt on
 *     the TS side). Valid range is 0..MPFR_FLAGS_ALL (= 63); the driver
 *     never emits a mask outside that range.
 *   - output is a bare JSON boolean emitted via jl_output_scalar_bool.
 *     Wire `true` iff (mask & MPFR_FLAGS_UNDERFLOW) != 0.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  20  (single-flag-set cases; both polarities)
 *   edge         :  32  (all 6 single flags; pairs; the empty/full masks)
 *   adversarial  :  12  (interactions with naturally-raised flags from real ops)
 *   fuzz         :  50  (PRNG-driven mask in [0, MPFR_FLAGS_ALL])
 *   mined        :   8  (transcribed shapes from mpfr/tests/texceptions.c L327-L351)
 *   ------------ ----
 *   total        : 122
 *
 * NOTE: this driver MUTATES libmpfr's global flag register across
 * cases. That's fine because the C process runs to completion in one
 * invocation; cases are serialised. The TS-side port runs in a fresh
 * worker per case (eval/harness/worker.ts), so each TS test starts from
 * a clean module-level flag state -- the mask input must drive the
 * pre-state explicitly.
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/exceptions.c L348-L355 -- C reference.
 * Ref: src/internal/mpfr/flags.ts -- expected TS flag-state module (not yet ported).
 * Ref: /usr/include/mpfr.h L77-L88 -- MPFR_FLAGS_* bit values.
 * Ref: mpfr/tests/texceptions.c -- mined source.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_divby0_p golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* The bit this predicate reads. Hard-coded; one-of:
 *   MPFR_FLAGS_UNDERFLOW = 1
 *   MPFR_FLAGS_OVERFLOW  = 2
 *   MPFR_FLAGS_NAN       = 4
 *   MPFR_FLAGS_INEXACT   = 8
 *   MPFR_FLAGS_ERANGE    = 16
 *   MPFR_FLAGS_DIVBY0    = 32
 */
#define PREDICATE_BIT MPFR_FLAGS_DIVBY0

/* Emit one case: set the global flag register to `mask`, call
 * mpfr_divby0_p(), emit JSONL. */
static inline void emit_case(FILE *out, const char *tag, uint64_t mask) {
    assert(mask <= MPFR_FLAGS_ALL);
    mpfr_clear_flags();
    mpfr_flags_set((mpfr_flags_t)mask);
    const uint64_t t0 = now_ns();
    const int got = mpfr_divby0_p();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_u64(out, 1, "mask", mask);
    jl_end_inputs(out);
    jl_output_scalar_bool(out, got);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 20 -- the predicate's truth table for each single bit
     * plus a few common polyfill cases. */
    /* ============================================================== */
    emit_case(out, "happy", 0);                              /* all clear */
    emit_case(out, "happy", PREDICATE_BIT);                  /* just our bit */
    emit_case(out, "happy", MPFR_FLAGS_UNDERFLOW);           /* the bit */
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW);            /* not the bit */
    emit_case(out, "happy", MPFR_FLAGS_NAN);                 /* not the bit */
    emit_case(out, "happy", MPFR_FLAGS_INEXACT);             /* not the bit */
    emit_case(out, "happy", MPFR_FLAGS_ERANGE);              /* not the bit */
    emit_case(out, "happy", MPFR_FLAGS_DIVBY0);              /* not the bit */
    emit_case(out, "happy", MPFR_FLAGS_ALL);                 /* all set */
    emit_case(out, "happy", MPFR_FLAGS_ALL ^ PREDICATE_BIT); /* all except ours */
    /* pairs that include our bit (set -> true) */
    emit_case(out, "happy", PREDICATE_BIT | MPFR_FLAGS_OVERFLOW);
    emit_case(out, "happy", PREDICATE_BIT | MPFR_FLAGS_NAN);
    emit_case(out, "happy", PREDICATE_BIT | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", PREDICATE_BIT | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", PREDICATE_BIT | MPFR_FLAGS_DIVBY0);
    /* pairs that exclude our bit (clear -> false) */
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN);
    emit_case(out, "happy", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_DIVBY0);
    emit_case(out, "happy", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);
    emit_case(out, "happy", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "happy", MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);

    /* ============================================================== */
    /* edge: 32 -- exhaustive single-bit and the empty/full/inversion
     * triples; covers the per-bit truth table. */
    /* ============================================================== */
    /* Each single bit isolated (6). */
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_DIVBY0);
    /* All 6 single-bit-inverted (6). */
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_UNDERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_OVERFLOW);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0);
    /* Triple-flag combinations covering all C(6,3) = 20 cases. */
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_INEXACT | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);
    emit_case(out, "edge", MPFR_FLAGS_NAN | MPFR_FLAGS_INEXACT | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_NAN | MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);
    emit_case(out, "edge", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE | MPFR_FLAGS_DIVBY0);

    /* ============================================================== */
    /* adversarial: 12 -- naturally-raised flags from real ops (where
     * libmpfr sets the flag itself, not the driver via mpfr_flags_set).
     * For these we don't pre-set; we drive a real underflow/overflow
     * and use the *resulting* __gmpfr_flags value as mask.
     *
     * The pattern is: clear, do an op that raises a specific flag,
     * READ __gmpfr_flags into a variable, clear again, set_flags(that),
     * call predicate. This tests the same code path but exercises the
     * full natural-flag-raising semantics. */
    /* ============================================================== */
    {
        mpfr_t a, b, c;
        mpfr_init2(a, 53); mpfr_init2(b, 53); mpfr_init2(c, 53);

        /* Drive an underflow via mpfr_set_emin then a tiny mul. */
        mpfr_exp_t saved_emin = mpfr_get_emin();
        mpfr_set_emin(-10);
        mpfr_clear_flags();
        mpfr_set_d(b, 1e-3, MPFR_RNDN);
        mpfr_set_d(c, 1e-3, MPFR_RNDN);
        mpfr_mul(a, b, c, MPFR_RNDN);  /* should underflow */
        uint64_t mask = (uint64_t)mpfr_flags_save();
        mpfr_set_emin(saved_emin);
        emit_case(out, "adversarial", mask);

        /* Drive an overflow via small emax. */
        mpfr_exp_t saved_emax = mpfr_get_emax();
        mpfr_set_emax(10);
        mpfr_clear_flags();
        mpfr_set_d(b, 1e3, MPFR_RNDN);
        mpfr_set_d(c, 1e3, MPFR_RNDN);
        mpfr_mul(a, b, c, MPFR_RNDN);  /* should overflow */
        mask = (uint64_t)mpfr_flags_save();
        mpfr_set_emax(saved_emax);
        emit_case(out, "adversarial", mask);

        /* Drive inexact via 1/3 in low precision. */
        mpfr_clear_flags();
        mpfr_set_ui(b, 1, MPFR_RNDN);
        mpfr_set_ui(c, 3, MPFR_RNDN);
        mpfr_div(a, b, c, MPFR_RNDN);
        mask = (uint64_t)mpfr_flags_save();
        emit_case(out, "adversarial", mask);

        /* Drive divby0. */
        mpfr_clear_flags();
        mpfr_set_ui(b, 1, MPFR_RNDN);
        mpfr_set_zero(c, +1);
        mpfr_div(a, b, c, MPFR_RNDN);  /* sets DIVBY0 */
        mask = (uint64_t)mpfr_flags_save();
        emit_case(out, "adversarial", mask);

        /* Drive NaN flag via 0/0. */
        mpfr_clear_flags();
        mpfr_set_zero(b, +1);
        mpfr_set_zero(c, +1);
        mpfr_div(a, b, c, MPFR_RNDN);
        mask = (uint64_t)mpfr_flags_save();
        emit_case(out, "adversarial", mask);

        /* Drive erange via mpfr_get_si on +Inf. */
        mpfr_clear_flags();
        mpfr_set_inf(b, +1);
        (void)mpfr_get_si(b, MPFR_RNDN);
        mask = (uint64_t)mpfr_flags_save();
        emit_case(out, "adversarial", mask);

        /* Compound: real underflow then driver-set extra bits. */
        mpfr_set_emin(-10);
        mpfr_clear_flags();
        mpfr_set_d(b, 1e-3, MPFR_RNDN);
        mpfr_set_d(c, 1e-3, MPFR_RNDN);
        mpfr_mul(a, b, c, MPFR_RNDN);
        mask = (uint64_t)mpfr_flags_save() | MPFR_FLAGS_ERANGE;
        mpfr_set_emin(saved_emin);
        emit_case(out, "adversarial", mask);

        /* Compound: real overflow OR with NaN. */
        mpfr_set_emax(10);
        mpfr_clear_flags();
        mpfr_set_d(b, 1e3, MPFR_RNDN);
        mpfr_set_d(c, 1e3, MPFR_RNDN);
        mpfr_mul(a, b, c, MPFR_RNDN);
        mask = (uint64_t)mpfr_flags_save() | MPFR_FLAGS_NAN;
        mpfr_set_emax(saved_emax);
        emit_case(out, "adversarial", mask);

        /* The exact boundary cases: just-the-bit, all-but-the-bit. */
        emit_case(out, "adversarial", PREDICATE_BIT);
        emit_case(out, "adversarial", MPFR_FLAGS_ALL & ~PREDICATE_BIT);

        /* Same bit twice (test that flag bits are sticky / set semantics). */
        mpfr_clear_flags();
        mpfr_flags_set(PREDICATE_BIT);
        mpfr_flags_set(PREDICATE_BIT);
        mask = (uint64_t)mpfr_flags_save();
        emit_case(out, "adversarial", mask);

        /* Sequence: set ALL, then flags_clear() everything except our bit. */
        mpfr_clear_flags();
        mpfr_flags_set(MPFR_FLAGS_ALL);
        mpfr_flags_clear(MPFR_FLAGS_ALL & ~PREDICATE_BIT);
        mask = (uint64_t)mpfr_flags_save();
        emit_case(out, "adversarial", mask);

        mpfr_clear(a); mpfr_clear(b); mpfr_clear(c);
    }

    /* ============================================================== */
    /* fuzz: 50 -- PRNG-driven mask in [0, MPFR_FLAGS_ALL]. */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xD1B2007D1B2007D1ULL);  /* D1B2007D seed (divby0_p) */
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t mask = xs64_below(&rng, (uint64_t)MPFR_FLAGS_ALL + 1);
            emit_case(out, "fuzz", mask);
        }
    }

    /* ============================================================== */
    /* mined: 8 -- patterns from mpfr/tests/texceptions.c L327-L351.
     * That section sets each flag in sequence and asserts each *_p
     * returns the expected value. We mine the "single-flag-set,
     * check-each-predicate" shape. */
    /* ============================================================== */
    emit_case(out, "mined", 0);                                /* baseline: nothing set */
    emit_case(out, "mined", MPFR_FLAGS_OVERFLOW);              /* L329-330 */
    emit_case(out, "mined", MPFR_FLAGS_UNDERFLOW);             /* L331-332 */
    emit_case(out, "mined", MPFR_FLAGS_DIVBY0);                /* L333-334 */
    emit_case(out, "mined", MPFR_FLAGS_NAN);                   /* L335-336 */
    emit_case(out, "mined", MPFR_FLAGS_ALL);                   /* L342 */
    emit_case(out, "mined", MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);  /* the 4-flag combo */
    emit_case(out, "mined", MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE);  /* the 2 not exercised in L329-336 */

    return 0;
}
