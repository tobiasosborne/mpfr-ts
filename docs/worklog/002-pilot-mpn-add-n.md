# 002 — Pilot Steps 6–10: `mpn_add_n` end-to-end

## Context

After Step 5 (harness skeleton, worklog 001) the Pilot still needed to
prove the harness against a real function. This shard covers Steps
6 through 10 — spec, golden, reference port, mutation-prove,
prompt template, live sonnet L3 attempt.

## What changed

- **Step 6** — `eval/functions/mpn_add_n/{spec.json, golden_driver.c,
  golden.jsonl}`. Driver compiles `-O2 -std=c11 -Wall -Wextra -Werror`,
  emits 152 deterministic cases (happy 25 + edge 32 + adversarial 15 +
  fuzz 80; mined omitted — `grep -rn mpn_add_n mpfr/tests/` finds none).
  All four required tag classes per Rule 7 minimums.
- **Step 7** — `src/internal/mpn/add_n.ts` (production substrate port,
  132 lines) + `eval/reference_ports/{correct,broken}/mpn_add_n.ts`.
  Broken variant uses option (i) "carry dropped between limbs"
  (`carry = 0n` reset inside the loop).
- **Step 8** — mutation-prove spot-grade:
  - correct: composite **1.0000**, 152/152 PASS
  - broken: composite **0.3950**, 36/152 PASS (no-carry cases happen to
    pass with the bug)
  - gap = **0.605** (need ≥ 0.49). PASS.
- **Side fix** — `runner.ts` `pathToUrl()` resolved relative paths
  against `import.meta.url` (the runner's own dir), so
  `--port src/internal/mpn/add_n.ts` from repo root failed with
  "module not found". Fixed to resolve against `process.cwd()`.
  Verified: relative paths work; Step 5 acceptance still 5/5 GREEN.
  `bd mpfr-ts-61i` closed.
- **Step 9** — `eval/driver/prompts.py` (518 lines) + `eval/driver/ralph.py`
  (140 lines). Dry-run renders a 35.7 KB prompt with all required tokens
  (MPFR, mpn_add_n, runner.ts, composite_correctness, Hallucination-risk).
- **Step 10** — `python3 eval/driver/ralph.py --function mpn_add_n
  --dry-run > /tmp/eval_mpn_add_n/prompt.txt`, then dispatched one
  sonnet L3 subagent with the prompt as task. Restricted reads:
  agent could NOT consult `src/internal/mpn/add_n.ts`,
  `eval/reference_ports/`, or `golden.jsonl` directly. Result:
  **composite=1.0000 on iteration 1**. Independently re-graded: 152/152
  PASS. state.db updated (functions row done; runs row inserted).

## Why these choices

- **Broken bug variant (i)** dropped inter-limb carry rather than e.g.
  wrong mask width. Option (i) preserves the "looks like a real port"
  property — easy to miss in review — while still failing every
  carry-propagating case. Option (ii) (63-bit mask) would fail almost
  everything trivially.
- **Substrate-exemption was already in `runner.ts`** (`portClass !==
  'substrate'` controls `requireCoreImport`). One fewer fix needed.
- **Read-restriction on the sonnet agent** was added orchestrator-side
  (extra notes appended to the prompt-file pointer in the Agent
  dispatch). Without it, sonnet could open `add_n.ts` directly and
  the eval reduces to a file-copy test. Even so, the prompt embeds the
  reference as a worked example — see the caveat below.

## Frictions surfaced

1. **Worked-example-eval-leak for function #1**: The prompt template
   (Step 9) embeds `src/internal/mpn/add_n.ts` verbatim as the worked
   example. For `mpn_add_n` this IS the target function — so
   "porting" was effectively "transcribe + add my own proof
   commentary". Composite=1.0 is real (the port grades clean), but
   the run does not test sonnet's algorithmic-porting capability.
   - For function #2 onward, the worked example will continue to be
     `mpn_add_n` while the target differs; the leak is gone naturally.
   - Filed as `bd mpfr-ts-???` — pending decision.
2. **state.db `runs` table requires non-null `perf_grade` and `usd_est`**.
   We don't measure either yet. Set both to `0.0`. Worth either making
   them nullable in the schema (next revision) or wiring a perf
   measurement path before too many runs accumulate with `perf_grade=0`
   as a non-signal.
3. **Pilot completion**: Per PIL.5, Pilot transitions to Production
   only after 10/10 pilot functions reach composite ≥ 0.95 in a single
   clean run. We have 1/10. Continuing in halt-on-failure mode.

## Acceptance

- `bun x tsc --noEmit` → clean
- `bun eval/acceptance/step5/run.ts` → 5/5 PASS
- `bun eval/harness/runner.ts --function mpn_add_n --port /tmp/eval_mpn_add_n/port.ts ... --class substrate` → composite=1.0000, 152/152
- `sqlite3 eval/state.db "SELECT name, status, best_correctness FROM functions WHERE name='mpn_add_n'"` → `mpn_add_n|done|1.0`

## Pointers

- `eval/functions/mpn_add_n/` — spec + driver (golden.jsonl gitignored)
- `src/internal/mpn/add_n.ts` — production port
- `eval/driver/{prompts.py, ralph.py}` — the renderer + driver
- `/tmp/eval_mpn_add_n/{prompt.txt, port.ts, grade_iter1.json, grade_verify.json}` — session artifacts

## Next

Function #2: `mpn_sub_n`. Same shape as `mpn_add_n` (substrate,
subtract-with-borrow instead of add-with-carry). The worked example
in the prompt will now differ from the target — first honest test of
sonnet's porting capability.
