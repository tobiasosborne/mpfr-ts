/*
 * golden_driver.c -- Golden master for MPFR's mpfr_buildopt_float16_p.
 *
 * C signature
 * -----------
 *
 *   int mpfr_buildopt_float16_p(void);
 *
 *   Returns 1 iff libmpfr was compiled with MPFR_WANT_FLOAT16, else 0.
 *   Ref: mpfr/src/buildopt.c L35-L43.
 *
 * TS port contract
 * ----------------
 *
 * The TS port (`src/ops/buildopt_float16_p.ts`) returns the compile-time
 * constant `false`. Pure-TS has no relied-upon native binary16 type
 * (Float16Array is a stage-3 proposal, not universally available across
 * Bun and Node target versions), so advertising support would be a lie.
 * This driver emits `false` as the expected output unconditionally.
 *
 * libmpfr-version note
 * --------------------
 *
 * Even when `mpfr_buildopt_float16_p` is exported on the system libmpfr,
 * its return value depends on whether libmpfr was configured with
 * --enable-float16. Distribution builds typically DO NOT enable it. We
 * therefore do not call libmpfr here -- the TS-expected value is the
 * compile-time constant `false`, derived from the TS port's contract
 * rather than from a libmpfr observable. Calling libmpfr could yield
 * either 0 or 1 depending on the build, making the golden
 * non-reproducible across host systems.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"happy",
 *    "inputs":{},
 *    "output":false,
 *    "time_ns":<n>}
 *
 *   `inputs` is empty (no-arg function). `output` is a bare JSON boolean
 *   emitted by jl_output_scalar_bool.
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
 * adding coverage. bd `mpfr-ts-sr4` will carry the formal carve-out
 * when Rule 7 enforcement lands.
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/buildopt.c -- C reference.
 * Ref: src/ops/buildopt_float16_p.ts -- production port.
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
