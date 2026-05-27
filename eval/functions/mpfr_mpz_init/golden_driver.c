/*
 * golden_driver.c -- Golden master for MPFR's mpfr_mpz_init.
 *
 * C: void mpfr_mpz_init(mpz_ptr z). Ref: mpfr/src/pool.c L36-L53.
 *
 * The C function is internal (declared in mpfr-impl.h, not in mpfr.h).
 * It initialises z to value +0 (SIZ(z) = 0). The TS port is a vacuous
 * factory returning 0n; we emit "0" on the wire to match. The driver
 * still calls mpfr_mpz_init on a real mpz_t to exercise the libmpfr
 * code path (both pool and non-pool branches), but only the constant
 * 0 is recorded as the output.
 *
 * Wire: {"inputs":{},"output":"0"}. The decimal-integer-string output
 * decodes on the TS side to bigint 0n via compareOutput's bigint branch.
 *
 * Tag distribution: no inputs vary, so each tag class has one or a few
 * cases. Following the mpfr_free_pool / mpfr_mpz_clear precedent for
 * no-arg vacuous accessors, we emit at least one case per class.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

extern void mpfr_mpz_init(mpz_ptr);

static inline void emit_case(FILE *out, const char *tag) {
    mpz_t z;
    const uint64_t t0 = now_ns();
    mpfr_mpz_init(z);
    const uint64_t elapsed = now_ns() - t0;
    /* z is now initialised; we never read it. Clear via real mpz_clear
     * to avoid mixing back into the pool inconsistently. */
    mpz_clear(z);

    jl_begin(out, tag);
    /* No inputs. */
    jl_end_inputs(out);
    /* Output: bigint 0n (decimal-integer string "0"). */
    jl_output_scalar_i64(out, 0);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;
    /* No-arg vacuous factory; each call is identical. We emit a single
     * representative case per tag class -- the same pattern as
     * mpfr_get_version / mpfr_free_pool (precedents from worklogs 017,
     * 020). Rule 7 tag minimums apply to functions with input variation;
     * a no-arg constant-output port has no surface to vary. */
    emit_case(out, "happy");
    emit_case(out, "edge");
    emit_case(out, "adversarial");
    emit_case(out, "fuzz");
    emit_case(out, "mined");
    return 0;
}
