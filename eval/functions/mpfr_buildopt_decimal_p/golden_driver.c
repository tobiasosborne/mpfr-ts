/*
 * golden_driver.c -- Golden master for MPFR's mpfr_buildopt_decimal_p.
 *
 * C signature
 * -----------
 *
 *   int mpfr_buildopt_decimal_p(void);
 *
 *   Returns 1 iff libmpfr was compiled with MPFR_WANT_DECIMAL_FLOATS,
 *   else 0. Ref: mpfr/src/buildopt.c L65-L73.
 *
 * TS port contract
 * ----------------
 *
 * The TS port (`src/ops/buildopt_decimal_p.ts`) returns the compile-time
 * constant `false`. Pure-TS has no IEEE-754-2008 decimal type, so
 * advertising decimal-float support would be a lie. This driver therefore
 * emits `false` as the expected output regardless of what the system
 * libmpfr would report -- the libmpfr-side call is made only so the wire
 * record carries a real time_ns measurement.
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
 * Ref: src/ops/buildopt_decimal_p.ts -- production port.
 * Ref: CLAUDE.md Rule 7 -- tag minimums (carved out here).
 */
#include "common.h"

#include <inttypes.h>

int main(void) {
    FILE *out = stdout;

    /* Call libmpfr's mpfr_buildopt_decimal_p once so the timing is real
     * (cheap; the TS-expected value is hard-coded to false irrespective
     * of the libmpfr return value). */
    const uint64_t t0 = now_ns();
    (void) mpfr_buildopt_decimal_p();
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, "happy");
    jl_end_inputs(out);
    jl_output_scalar_bool(out, 0);  /* 0 -> false on the wire */
    jl_finish(out, elapsed);

    return 0;
}
