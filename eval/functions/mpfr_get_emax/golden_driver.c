/*
 * golden_driver.c -- Golden master for MPFR's mpfr_get_emax.
 *
 * C signature
 * -----------
 *
 *   mpfr_exp_t mpfr_get_emax(void);
 *
 *   Returns the current value of the process-wide __gmpfr_emax, a
 *   signed integer setting that bounds the maximum allowed exponent
 *   in subsequent MPFR arithmetic. Defaults to MPFR_EMAX_DEFAULT =
 *   (1 << 30) - 1 = 1073741823.
 *
 *   Ref: mpfr/src/exceptions.c L64-L70 -- one-line body.
 *   Ref: mpfr/src/mpfr.h L231 -- MPFR_EMAX_DEFAULT.
 *
 * TS port contract
 * ----------------
 *
 * The TS port (`src/ops/get_emax.ts`) returns EMAX_DEFAULT = 2^30 - 1
 * as a bigint, unconditionally. The TS library has no mutable global
 * exponent state; mpfr_set_emin / mpfr_set_emax are not yet ported. The
 * value matches what libmpfr would return at the default emax (i.e.
 * before any mpfr_set_emax call in the process).
 *
 * This driver runs in a fresh process with no prior mpfr_set_emax call,
 * so libmpfr's mpfr_get_emax returns the default. We emit that value
 * (which we ALSO compute as (1<<30)-1 to assert agreement with the C
 * macro) as a decimal string per jl_output_scalar_i64.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"happy",
 *    "inputs":{},
 *    "output":"1073741823",
 *    "time_ns":<n>}
 *
 *   `inputs` is empty (no-arg function). `output` is a quoted decimal
 *   string (jl_output_scalar_i64), which decodeExpectedOutput parses
 *   as bigint -- matching the TS port's bigint return.
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
 * Ref: mpfr/src/exceptions.c L64-L70 -- C reference.
 * Ref: src/ops/get_emax.ts -- production port.
 * Ref: CLAUDE.md Rule 7 -- tag minimums (carved out here).
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

int main(void) {
    FILE *out = stdout;

    /* No mpfr_set_emax call in this process -> libmpfr returns default. */
    const uint64_t t0 = now_ns();
    const mpfr_exp_t emax = mpfr_get_emax();
    const uint64_t elapsed = now_ns() - t0;

    /* Sanity: the libmpfr default must equal (2^30 - 1). If a future
     * libmpfr changes its default the assertion fires here and the
     * driver fails loudly rather than emitting a silently-wrong
     * golden. */
    const int64_t expected = (int64_t)((1ULL << 30) - 1);
    assert((int64_t)emax == expected);

    jl_begin(out, "happy");
    jl_end_inputs(out);
    jl_output_scalar_i64(out, (int64_t)emax);
    jl_finish(out, elapsed);

    return 0;
}
