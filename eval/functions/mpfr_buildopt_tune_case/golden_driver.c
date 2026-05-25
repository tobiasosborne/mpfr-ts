/*
 * golden_driver.c -- Golden master for MPFR's mpfr_buildopt_tune_case.
 *
 * C signature
 * -----------
 *
 *   const char *mpfr_buildopt_tune_case(void);
 *
 *   Returns a pointer to a static string describing the libmpfr build's
 *   tune-case. Always defined (default 'default').
 *   Ref: mpfr/src/buildopt.c L96-L100.
 *
 * TS port contract
 * ----------------
 *
 * The TS port (`src/ops/buildopt_tune_case.ts`) returns the constant
 * string 'default'. Pure-TS has no per-platform tuning. The golden
 * emits the TS-expected string unconditionally; we do NOT call libmpfr
 * (which would return the system's mparam.h tune value, varying by
 * host).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"happy",
 *    "inputs":{},
 *    "output":"default",
 *    "time_ns":<n>}
 *
 * Tag distribution (Rule 7 carve-out)
 * -----------------------------------
 *
 * No-arg accessor; carved out per the mpfr_buildopt_float128_p pattern.
 * Single happy case is the full test surface. Note carve-out in spec.json.
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/buildopt.c -- C reference.
 * Ref: src/ops/buildopt_tune_case.ts -- production port.
 * Ref: CLAUDE.md Rule 7 -- tag minimums (carved out here).
 */
#include "common.h"

#include <inttypes.h>

int main(void) {
    FILE *out = stdout;

    const uint64_t elapsed = 0;

    jl_begin(out, "happy");
    jl_end_inputs(out);
    jl_output_scalar_str(out, "default");
    jl_finish(out, elapsed);

    return 0;
}
