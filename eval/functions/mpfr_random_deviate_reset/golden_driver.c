/*
 * golden_driver.c -- Golden master for mpfr_random_deviate_reset.
 *
 * Driver synthesizes (e, h, f) triples (NOT calling libmpfr for the
 * state generation since the struct is private), then emits each as
 * an input with the expected output of {e:0, h:input.h, f:input.f}.
 *
 * Wire: {"inputs":{"x":{"e":...,"h":...,"f":...}},"output":{"e":"0","h":...,"f":...}}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <inttypes.h>

static inline void emit_case(FILE *out, const char *tag,
                              uint64_t e, uint64_t h, uint64_t f) {
    const uint64_t t0 = now_ns();
    /* Pure synthesis -- no libmpfr call since the struct is private and
     * the reset operation is trivial. */
    const uint64_t out_e = 0;
    const uint64_t out_h = h;
    const uint64_t out_f = f;
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    fprintf(out, "\"x\":{\"e\":\"%" PRIu64 "\",\"h\":\"%" PRIu64 "\",\"f\":\"%" PRIu64 "\"}",
            e, h, f);
    jl_end_inputs(out);
    fprintf(out, ",\"output\":{\"e\":\"%" PRIu64 "\",\"h\":\"%" PRIu64 "\",\"f\":\"%" PRIu64 "\"}",
            out_e, out_h, out_f);
    jl_finish(out, elapsed);
}

int main(void) {
    FILE *out = stdout;
    /* happy: 20 */
    emit_case(out, "happy", 0, 0, 0);
    emit_case(out, "happy", 32, 0, 0);
    emit_case(out, "happy", 32, 0xFFFFFFFFULL, 0);
    emit_case(out, "happy", 64, 1, 0);
    emit_case(out, "happy", 64, 12345, 0);
    emit_case(out, "happy", 96, 1, 100);
    emit_case(out, "happy", 128, 1, 1000);
    emit_case(out, "happy", 32, 0xCAFEBEEEULL, 0);
    emit_case(out, "happy", 64, 0xDEADBEEFULL, 0);
    emit_case(out, "happy", 256, 7, 99);
    emit_case(out, "happy", 512, 123456, 7890);
    emit_case(out, "happy", 1024, 1, 2);
    emit_case(out, "happy", 2048, 0xABCDEF01ULL, 12345678);
    emit_case(out, "happy", 32, 0xFEDCBA98ULL, 0);
    emit_case(out, "happy", 64, 0xFFFFFFFEULL, 0);
    emit_case(out, "happy", 96, 0xAAAAAAAAULL, 42);
    emit_case(out, "happy", 128, 1, 99999);
    emit_case(out, "happy", 32, 0, 0);
    emit_case(out, "happy", 64, 0, 0);
    emit_case(out, "happy", 96, 0, 0);
    /* edge: 30 */
    emit_case(out, "edge", 1, 0, 0);
    emit_case(out, "edge", 2, 1, 0);
    emit_case(out, "edge", 31, 0, 0);
    emit_case(out, "edge", 32, 0xFFFFFFFFULL, 0);
    emit_case(out, "edge", 33, 1, 1);
    emit_case(out, "edge", 63, 0, 0);
    emit_case(out, "edge", 64, 0xFFFFFFFFULL, 0xFFFFFFFFULL);
    emit_case(out, "edge", 65, 0, 1);
    emit_case(out, "edge", 95, 0, 0);
    emit_case(out, "edge", 96, 0, 0xFFFFFFFFFFULL);
    emit_case(out, "edge", 97, 0, 0);
    emit_case(out, "edge", 127, 0, 0);
    emit_case(out, "edge", 128, 0, 0xFFFFFFFFFFFFULL);
    emit_case(out, "edge", 129, 0, 0);
    emit_case(out, "edge", 255, 0, 0);
    emit_case(out, "edge", 256, 0xCDCDCDCDULL, 0xABABABABULL);
    emit_case(out, "edge", 257, 0, 0);
    emit_case(out, "edge", 1000, 0, 0);
    emit_case(out, "edge", 2000, 0, 0);
    emit_case(out, "edge", 4096, 0, 0);
    emit_case(out, "edge", 8192, 0, 0);
    emit_case(out, "edge", 65536, 0, 0);
    emit_case(out, "edge", 1, 0, 0);
    emit_case(out, "edge", 2, 0, 0);
    emit_case(out, "edge", 100, 0x12345678ULL, 0xABCDEF01ULL);
    emit_case(out, "edge", 200, 0xDEADBEEFULL, 0xCAFEBABEULL);
    emit_case(out, "edge", 300, 0, 0);
    emit_case(out, "edge", 400, 0, 0xFFEEDDCCBBAA9988ULL);
    emit_case(out, "edge", 500, 0xFEDCBA98ULL, 0);
    emit_case(out, "edge", 600, 0xABCDABCDULL, 12345);
    /* adversarial: 12 */
    emit_case(out, "adversarial", 0xFFFFFFFFULL, 0, 0);
    emit_case(out, "adversarial", 0xFFFFFFFEULL, 0, 0);
    emit_case(out, "adversarial", 32, 1, 0);
    emit_case(out, "adversarial", 64, 1, 0xFFFFFFFFFFFFFFFFULL);
    emit_case(out, "adversarial", 65535, 1, 1);
    emit_case(out, "adversarial", 1, 1, 1);
    emit_case(out, "adversarial", 100000, 0xFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL);
    emit_case(out, "adversarial", 0, 1, 1);
    emit_case(out, "adversarial", 0, 0xFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL);
    emit_case(out, "adversarial", 32, 0xAAAA5555ULL, 0);
    emit_case(out, "adversarial", 32, 0x5555AAAAULL, 0);
    emit_case(out, "adversarial", 32, 1, 0);
    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xDEEDABBA00ABBA00ULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t e = xs64_below(&rng, 10000);
            const uint64_t h = xs64_next(&rng) & 0xFFFFFFFFULL;
            const uint64_t f = xs64_next(&rng);
            emit_case(out, "fuzz", e, h, f);
        }
    }
    /* mined: 5 -- trandom_deviate.c patterns. */
    emit_case(out, "mined", 0, 0, 0);
    emit_case(out, "mined", 32, 0xDEADBEEFULL, 0);
    emit_case(out, "mined", 64, 1, 12345);
    emit_case(out, "mined", 100, 0xCAFEBABEULL, 0xABCDEF01ULL);
    emit_case(out, "mined", 1000, 0, 0);
    return 0;
}
