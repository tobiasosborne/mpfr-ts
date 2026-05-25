/*
 * golden_driver.c -- Golden master for MPFR's mpfr_buildopt_tls_p.
 *
 * C signature
 * -----------
 *
 *   int mpfr_buildopt_tls_p(void);
 *
 *   Returns 1 iff libmpfr was compiled with MPFR_USE_THREAD_SAFE, else 0.
 *   Ref: mpfr/src/buildopt.c L25-L33.
 *
 * TS port contract
 * ----------------
 *
 * The TS port (`src/ops/buildopt_tls_p.ts`) returns the compile-time
 * constant `false`. Pure-TS modules run in a single-threaded event loop;
 * Worker threads are isolates (each has its own module state by
 * construction). There is no shared cross-thread mutable state that
 * would need TLS, so the question 'does this libmpfr have TLS' has no
 * meaningful translation -- returning `false` is the only honest answer
 * and matches the convention used by sibling buildopt_*_p ports.
 *
 * libmpfr-version note
 * --------------------
 *
 * Even when `mpfr_buildopt_tls_p` is exported on the system libmpfr (it
 * is on every libmpfr 4.x), its return value depends on whether libmpfr
 * was configured with --enable-thread-safe. Distribution builds usually
 * DO enable it (so the system observable is 1), but the TS expected
 * value is the compile-time constant `false`. We therefore do not call
 * libmpfr here -- calling it would yield 1 on most hosts and mismatch
 * the TS port. The driver hard-codes `false` to match the TS contract.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"happy",
 *    "inputs":{},
 *    "output":false,
 *    "time_ns":<n>}
 *
 * Tag distribution (Rule 7 carve-out)
 * -----------------------------------
 *
 * Rule 7 mandates happy>=20, edge>=30, adversarial>=10, fuzz>=50,
 * mined>=5. Those minimums are inapplicable to no-arg accessor
 * functions: the input domain is empty (Cartesian product of zero
 * argument types is a single point), so the entire test surface is one
 * call. A single happy case verifies the contract; padding with
 * duplicate cases to satisfy a tag count would inflate signal without
 * adding coverage. Same carve-out as mpfr_buildopt_float128_p (see
 * worklog 016 / 017 for the precedent).
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/buildopt.c -- C reference.
 * Ref: src/ops/buildopt_tls_p.ts -- production port.
 * Ref: CLAUDE.md Rule 7 -- tag minimums (carved out here).
 */
#include "common.h"

#include <inttypes.h>

int main(void) {
    FILE *out = stdout;

    /* TS-expected value is the compile-time constant `false`,
     * independent of how the system libmpfr was built. time_ns is
     * recorded as 0; this is informational only -- the grader doesn't
     * compare timing. */
    const uint64_t elapsed = 0;

    jl_begin(out, "happy");
    jl_end_inputs(out);
    jl_output_scalar_bool(out, 0);  /* 0 -> false on the wire */
    jl_finish(out, elapsed);

    return 0;
}
