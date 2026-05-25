# 019 — value_codec scalar-string outputs (mpfr-ts-2ls)

> Chunk 1 of a 3-chunk continuation session. Fixed the value_codec
> scalar-string carve-out that worklog 018 surfaced. Shipped 2 prod
> ports (fdump + buildopt_tune_case) at composite=1.0. ~25 min, ~$1.

## TL;DR

| Change | Path | Lines |
|---|---|---:|
| Codec passthrough + string arm | `eval/harness/value_codec.ts` | +9 / -1 |
| JSON-escape helper rewrite | `eval/golden_master/common.h` | +25 / -8 (2 fns updated identically) |
| RED-GREEN test | `eval/harness/value_codec.test.ts` (NEW) | +75 |
| Reference port fix | `eval/reference_ports/correct/mpfr_fdump.ts` | -8 |
| Production ports | `src/ops/fdump.ts`, `src/ops/buildopt_tune_case.ts` | NEW |

State.db: 174 → **176 done · 19 blocked · 0 pending**.

## Process: strict RED-GREEN, 5 serial subagents

1. **INVESTIGATE** (subagent 1, ~8 min): mapped exact 9-line codec diff, 25-line common.h rewrite, scan of all 3 jl_output_scalar_str users (fdump, tune_case, get_z), identified test framework (bun test, mirroring ast_check.test.ts).
2. **RED-GREEN codec** (subagent 2, ~5 min): wrote 7 bun tests at value_codec.test.ts → RED → applied minimal diff → GREEN. Regression: 18/18 ast_check tests, 123/123 pytest, 3 spot-grades of shipped ports (mpfr_eq, mpfr_add, mpfr_get_z) all composite=1.0.
3. **RED-GREEN helper** (subagent 3, ~5 min): proved fdump golden was malformed JSON → upgraded jl_output_scalar_str + jl_kv_str → regenerated 6 affected goldens (fdump, tune_case, get_z, print_rnd_mode, powerof2_raw2, set_z) → fdump JSON now parses cleanly → mpfr_get_z still composite=1.0 (decimal-string outputs byte-identical).
4. **REFERENCE PORT FIX** (orchestrator inline, ~2 min): after the helper landed and fdump's golden became parseable, the reference port mismatched 48/117 cases. Root cause: ref port stripped trailing zeros, but C source (dump.c L80-L92) emits exactly prec bits — the loop break happens AFTER emitting the prec-th bit. Removed the strip logic → composite jumped 0.59 → 1.0.
5. **PORT** (subagent 4, ~3 min): wrote `src/ops/fdump.ts` (117/117, mutate=killed with 2 clean) and `src/ops/buildopt_tune_case.ts` (1/1, mutate=vacuous).

Total wall time: ~25 min. Cost: ~$1.

## Why this matters

Worklog 018 carved out fdump + tune_case because the codec couldn't handle string outputs. That was the smallest possible carve-out (2 functions) but it represented a class of blockers — any future `mpfr_get_str` or other string-returning function would hit the same wall. The 9-LOC codec change + 25-LOC C helper rewrite unblocks the entire class.

The reference port bug surfaced was a real find: the calibration phase couldn't see it earlier because the malformed golden never reached the comparison phase. As soon as the golden was well-formed, the ref-port-vs-libmpfr divergence became visible. **Calibration discipline + working transport layer = bugs that would have shipped at composite=0.59 get caught.**

## Frictions

1. **3 jl_kv_str users (print_rnd_mode, powerof2_raw2, set_z) regenerated even though their output is byte-identical**. Just a regen-counter side effect; no semantic change.
2. **Subagent style edit drift**: one subagent accidentally modified `src/ops/get_emin.ts` (deleted a 3-line comment, no functional change). Reverted before commit. Worth a stricter "do not touch existing ports" reminder in PREP prompts.

## Pointers

- `eval/harness/value_codec.ts` L246-L356 (decodeExpectedOutput), L444-L488 (compareOutput)
- `eval/golden_master/common.h` L273-L298 (jl_kv_str), L587-L612 (jl_output_scalar_str)
- `eval/harness/value_codec.test.ts` (new test pattern for codec changes)
- bd `mpfr-ts-2ls` (CLOSED with reason in this session)
