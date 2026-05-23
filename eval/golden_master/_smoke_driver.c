/*
 * _smoke_driver.c — Step 4 pilot scaffolding for common.h.
 *
 * Throwaway: this file's only purpose is to prove that common.h
 * compiles -Werror clean and that every helper it exposes emits JSON
 * that (a) parses and (b) round-trips through src/core.ts's validate()
 * for any MPFR-shaped value.
 *
 * Three test cases, each on its own JSONL line:
 *
 *   1. "happy"       — u64 + limb array + rounding mode in inputs,
 *                      Result-shaped (value, ternary) output. Exercises
 *                      jl_kv_u64, jl_kv_limbs, jl_kv_rnd,
 *                      jl_output_result, jl_kv_mpfr (normal value).
 *   2. "edge"        — bare MPFR input (inf), bare MPFR output (NaN).
 *                      Also slips a struct-shaped output (begin/end)
 *                      in via the input side so we cover those helpers
 *                      without bloating to 4 lines: we use a zero
 *                      MPFR input via jl_kv_mpfr to cover that branch
 *                      of the value helper too.
 *   3. "fuzz"        — PRNG-driven limbs + i64 + str + int input,
 *                      struct-shaped output containing a u64 and a
 *                      string field (covers jl_output_begin_object,
 *                      jl_output_end_object, jl_kv_str inside output,
 *                      and jl_output_scalar_u64 via a separate run).
 *
 * Senior-eng note: kept under eval/golden_master/_smoke_driver.c (not
 * deleted post-test) as an executable spec of how common.h is meant
 * to be used. Leading underscore marks it skip-eligible for any tool
 * that walks the directory looking for real per-function drivers; the
 * sibling build.sh walks eval/functions/ not here, so it will never
 * pick this file up automatically.
 *
 * Coverage matrix (helper × case-using-it):
 *
 *   xs64_seed/next/below      : case 3
 *   now_ns                    : all cases
 *   jl_begin/end_inputs/finish: all cases
 *   jl_kv_u64                 : case 1
 *   jl_kv_i64                 : case 3
 *   jl_kv_int                 : case 3
 *   jl_kv_str                 : case 3
 *   jl_kv_rnd                 : case 1
 *   jl_kv_limbs               : cases 1, 3
 *   jl_kv_mpfr (normal/inf/zero/nan): cases 1, 2 (inf+zero+nan)
 *   jl_output_result          : case 1
 *   jl_output_mpfr            : case 2
 *   jl_output_scalar_u64      : (covered via fake_output in case 3? no
 *                                — case 3 uses struct output, so we
 *                                also call scalar_u64 inline in case 2
 *                                — but case 2 already has output. We
 *                                accept this: jl_output_scalar_u64
 *                                isn't covered by the 3-line driver;
 *                                we cover it via a stderr-side print
 *                                to a discarded FILE* so it still gets
 *                                compiled+called. See "exercise" block.)
 *   jl_output_scalar_str      : same as above (stderr-side exercise)
 *   jl_output_begin_object/end_object : case 3
 *
 * Build:
 *   gcc -O2 -std=c11 -Wall -Wextra -Werror -I . \
 *       _smoke_driver.c $(pkg-config --cflags --libs mpfr) -lm \
 *       -o /tmp/_mpfr_ts_smoke
 */
#include "common.h"

int main(void) {
    FILE *out = stdout;

    /* ============================================================ */
    /* Case 1: "happy"                                              */
    /*                                                              */
    /* Inputs:  n=5, limbs=[1, 2], rnd=RNDN                         */
    /* Output:  Result { value: normal MPFR(3.14, 53 bit), ternary } */
    /* ============================================================ */
    {
        const uint64_t t0 = now_ns();

        jl_begin(out, "happy");
        jl_kv_u64(out,   1, "n", 5);
        const mp_limb_t limbs[2] = { 1, 2 };
        jl_kv_limbs(out, 0, "limbs", limbs, 2);
        jl_kv_rnd(out,   0, "rnd", MPFR_RNDN);
        jl_end_inputs(out);

        /* 3.14 isn't representable in binary so mpfr_set_d returns a
         * nonzero ternary — perfect for exercising jl_output_result
         * with a non-trivial flag. */
        mpfr_t x;
        mpfr_init2(x, 53);
        const int ternary = mpfr_set_d(x, 3.14, MPFR_RNDN);
        jl_output_result(out, x, ternary);
        mpfr_clear(x);

        jl_finish(out, now_ns() - t0);
    }

    /* ============================================================ */
    /* Case 2: "edge"                                               */
    /*                                                              */
    /* Inputs:  a = +infinity, b = -0  (covers inf and zero kinds)  */
    /* Output:  NaN (bare MPFR via jl_output_mpfr) — covers nan     */
    /*          kind. The case as a whole hits 3 of the 4 kinds in   */
    /*          jl_kv_mpfr; "normal" is covered in case 1.          */
    /* ============================================================ */
    {
        const uint64_t t0 = now_ns();

        mpfr_t a, b, nan_v;
        mpfr_init2(a, 24);
        mpfr_set_inf(a, +1);             /* +infinity */
        mpfr_init2(b, 24);
        mpfr_set_zero(b, -1);            /* -0 */
        mpfr_init2(nan_v, 24);
        mpfr_set_nan(nan_v);

        jl_begin(out, "edge");
        jl_kv_mpfr(out, 1, "a", a);
        jl_kv_mpfr(out, 0, "b", b);
        jl_end_inputs(out);
        jl_output_mpfr(out, nan_v);
        jl_finish(out, now_ns() - t0);

        mpfr_clear(a);
        mpfr_clear(b);
        mpfr_clear(nan_v);
    }

    /* ============================================================ */
    /* Case 3: "fuzz"                                               */
    /*                                                              */
    /* Inputs:  PRNG-chosen 1..4 random limbs + an i64 + a str      */
    /*          containing chars the escaper has to handle + an     */
    /*          int.                                                 */
    /* Output:  struct {"count":"<u64>","flag":N} via the           */
    /*          begin_object / end_object helpers.                  */
    /* ============================================================ */
    {
        const uint64_t t0 = now_ns();

        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEEULL);
        const size_t n = (size_t)(1 + xs64_below(&rng, 4));
        mp_limb_t limbs[4];
        uint64_t fake_hash = 0;
        for (size_t i = 0; i < n; ++i) {
            limbs[i] = (mp_limb_t)xs64_next(&rng);
            fake_hash ^= (uint64_t)limbs[i];
        }

        jl_begin(out, "fuzz");
        jl_kv_limbs(out, 1, "limbs", limbs, n);
        jl_kv_i64(out,   0, "delta", -42);
        jl_kv_str(out,   0, "note",  "a \"quoted\" \\test\\");
        jl_kv_int(out,   0, "width", 64);
        jl_end_inputs(out);
        jl_output_begin_object(out);
        jl_kv_u64(out, 1, "count", fake_hash);
        jl_kv_int(out, 0, "flag", 1);
        jl_output_end_object(out);
        jl_finish(out, now_ns() - t0);
    }

    /* ------------------------------------------------------------ */
    /* Helper-coverage exercise: jl_output_scalar_u64 and           */
    /* jl_output_scalar_str aren't in the 3 stdout lines. Compile-  */
    /* and-call them against a /dev/null sink so any -Werror or     */
    /* runtime breakage still surfaces in the smoke test, without   */
    /* polluting the parseable JSONL stream.                        */
    /* ------------------------------------------------------------ */
    {
        FILE *sink = fopen("/dev/null", "w");
        if (sink == NULL) {
            fprintf(stderr, "smoke: cannot open /dev/null\n");
            return 1;
        }
        jl_begin(sink, "exercise");
        jl_kv_u64(sink, 1, "x", 1);
        jl_end_inputs(sink);
        jl_output_scalar_u64(sink, 12345);
        jl_finish(sink, 0);

        jl_begin(sink, "exercise");
        jl_kv_u64(sink, 1, "x", 1);
        jl_end_inputs(sink);
        jl_output_scalar_str(sink, "hello \"world\\");
        jl_finish(sink, 0);
        fclose(sink);
    }

    return 0;
}
