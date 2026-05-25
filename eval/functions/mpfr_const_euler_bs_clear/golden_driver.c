/*
 * golden_driver.c -- Golden master for mpfr_const_euler_bs_clear.
 *
 * The C function is `static`; we re-implement via the golden-driver-
 * substitute pattern (ADR 0002): emit a fully-zeroed state for any
 * input.
 *
 * Wire: {"inputs":{"s":{"P":...,"Q":...,...}},"output":{"P":"0",...,"V":"0"}}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <inttypes.h>

static inline void emit_case(FILE *out, const char *tag,
                              uint64_t P, uint64_t Q, uint64_t T,
                              uint64_t C, uint64_t D, uint64_t V) {
    const uint64_t t0 = now_ns();
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    fprintf(out, "\"s\":{\"P\":\"%" PRIu64 "\",\"Q\":\"%" PRIu64 "\","
                 "\"T\":\"%" PRIu64 "\",\"C\":\"%" PRIu64 "\","
                 "\"D\":\"%" PRIu64 "\",\"V\":\"%" PRIu64 "\"}",
            P, Q, T, C, D, V);
    jl_end_inputs(out);
    fputs(",\"output\":{\"P\":\"0\",\"Q\":\"0\",\"T\":\"0\",\"C\":\"0\",\"D\":\"0\",\"V\":\"0\"}", out);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;
    /* happy: 20 -- typical states. */
    emit_case(out, "happy", 0, 0, 0, 0, 0, 0);
    emit_case(out, "happy", 1, 1, 1, 1, 1, 1);
    emit_case(out, "happy", 2, 3, 5, 7, 11, 13);
    emit_case(out, "happy", 100, 200, 300, 400, 500, 600);
    emit_case(out, "happy", 1000, 2000, 3000, 4000, 5000, 6000);
    emit_case(out, "happy", 1, 0, 0, 0, 0, 0);
    emit_case(out, "happy", 0, 1, 0, 0, 0, 0);
    emit_case(out, "happy", 0, 0, 1, 0, 0, 0);
    emit_case(out, "happy", 0, 0, 0, 1, 0, 0);
    emit_case(out, "happy", 0, 0, 0, 0, 1, 0);
    emit_case(out, "happy", 0, 0, 0, 0, 0, 1);
    emit_case(out, "happy", 42, 42, 42, 42, 42, 42);
    emit_case(out, "happy", 17, 19, 23, 29, 31, 37);
    emit_case(out, "happy", 0xCAFEBABEULL, 0xDEADBEEFULL, 0xFEEDFACEULL, 0xABCDEF01ULL, 0xFEDCBA98ULL, 0x11223344ULL);
    emit_case(out, "happy", 100, 0, 100, 0, 100, 0);
    emit_case(out, "happy", 0, 100, 0, 100, 0, 100);
    emit_case(out, "happy", 0xFFFFFFFFULL, 0, 0xFFFFFFFFULL, 0, 0xFFFFFFFFULL, 0);
    emit_case(out, "happy", 1, 2, 4, 8, 16, 32);
    emit_case(out, "happy", 32, 16, 8, 4, 2, 1);
    emit_case(out, "happy", 1234567, 7654321, 11111, 22222, 33333, 44444);
    /* edge: 30 */
    emit_case(out, "edge", 1, 0, 0, 0, 0, 0);
    emit_case(out, "edge", 0, 1, 0, 0, 0, 0);
    emit_case(out, "edge", 0, 0, 1, 0, 0, 0);
    emit_case(out, "edge", 0, 0, 0, 1, 0, 0);
    emit_case(out, "edge", 0, 0, 0, 0, 1, 0);
    emit_case(out, "edge", 0, 0, 0, 0, 0, 1);
    emit_case(out, "edge", 0xFFFFFFFFULL, 0xFFFFFFFFULL, 0xFFFFFFFFULL, 0xFFFFFFFFULL, 0xFFFFFFFFULL, 0xFFFFFFFFULL);
    emit_case(out, "edge", 1, 1, 0, 0, 0, 0);
    emit_case(out, "edge", 0, 0, 1, 1, 0, 0);
    emit_case(out, "edge", 0, 0, 0, 0, 1, 1);
    emit_case(out, "edge", 1, 0, 1, 0, 1, 0);
    emit_case(out, "edge", 0, 1, 0, 1, 0, 1);
    emit_case(out, "edge", 100, 100, 100, 0, 0, 0);
    emit_case(out, "edge", 0, 0, 0, 100, 100, 100);
    emit_case(out, "edge", 999999, 1, 1, 1, 1, 1);
    emit_case(out, "edge", 1, 999999, 1, 1, 1, 1);
    emit_case(out, "edge", 1, 1, 999999, 1, 1, 1);
    emit_case(out, "edge", 1, 1, 1, 999999, 1, 1);
    emit_case(out, "edge", 1, 1, 1, 1, 999999, 1);
    emit_case(out, "edge", 1, 1, 1, 1, 1, 999999);
    emit_case(out, "edge", 0, 0, 0xFFFFFFFFULL, 0, 0, 0);
    emit_case(out, "edge", 0xAAULL, 0xBBULL, 0xCCULL, 0xDDULL, 0xEEULL, 0xFFULL);
    emit_case(out, "edge", 2, 0, 0, 0, 0, 0);
    emit_case(out, "edge", 0, 2, 0, 0, 0, 0);
    emit_case(out, "edge", 0, 0, 2, 0, 0, 0);
    emit_case(out, "edge", 0, 0, 0, 2, 0, 0);
    emit_case(out, "edge", 0, 0, 0, 0, 2, 0);
    emit_case(out, "edge", 0, 0, 0, 0, 0, 2);
    emit_case(out, "edge", 999, 888, 777, 666, 555, 444);
    emit_case(out, "edge", 444, 555, 666, 777, 888, 999);
    /* adversarial: 12 */
    emit_case(out, "adversarial", 0xFFFFFFFFFFFFFFFFULL, 0, 0, 0, 0, 0);
    emit_case(out, "adversarial", 0, 0xFFFFFFFFFFFFFFFFULL, 0, 0, 0, 0);
    emit_case(out, "adversarial", 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL);
    emit_case(out, "adversarial", 1, 0, 0, 0xFFFFFFFFULL, 0, 0);
    emit_case(out, "adversarial", 0, 1, 0, 0, 0xFFFFFFFFULL, 0);
    emit_case(out, "adversarial", 0xCAFEFACE0DECEED1ULL, 1, 1, 1, 1, 1);
    emit_case(out, "adversarial", 1, 0xCAFEFACE0DECEED1ULL, 1, 1, 1, 1);
    emit_case(out, "adversarial", 1, 1, 0xCAFEFACE0DECEED1ULL, 1, 1, 1);
    emit_case(out, "adversarial", 1, 1, 1, 0xCAFEFACE0DECEED1ULL, 1, 1);
    emit_case(out, "adversarial", 1, 1, 1, 1, 0xCAFEFACE0DECEED1ULL, 1);
    emit_case(out, "adversarial", 1, 1, 1, 1, 1, 0xCAFEFACE0DECEED1ULL);
    emit_case(out, "adversarial", 0xCAFEFACE0DECEED1ULL, 0xCAFEFACE0DECEED1ULL, 0xCAFEFACE0DECEED1ULL, 0xCAFEFACE0DECEED1ULL, 0xCAFEFACE0DECEED1ULL, 0xCAFEFACE0DECEED1ULL);
    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xEEDDAACCBBFFEE00ULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t P = xs64_next(&rng);
            const uint64_t Q = xs64_next(&rng);
            const uint64_t T = xs64_next(&rng);
            const uint64_t C = xs64_next(&rng);
            const uint64_t D = xs64_next(&rng);
            const uint64_t V = xs64_next(&rng);
            emit_case(out, "fuzz", P, Q, T, C, D, V);
        }
    }
    /* mined: 5 */
    emit_case(out, "mined", 0, 0, 0, 0, 0, 0);
    emit_case(out, "mined", 1, 1, 1, 1, 1, 1);
    emit_case(out, "mined", 100, 200, 300, 400, 500, 600);
    emit_case(out, "mined", 0xDEADBEEFULL, 0xCAFEBABEULL, 0xABABABABULL, 0xCDCDCDCDULL, 0xEFEFEFEFULL, 0xFEFEFEFEULL);
    emit_case(out, "mined", 1234567, 7654321, 11111, 22222, 33333, 44444);
    return 0;
}
