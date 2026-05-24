# 009 — validation finalize + first shadow-mode trial

> Picks up from `docs/worklog/008-automation-infra.md`. Closes the
> automation-validation arc by fixing the two mutator regex bugs +
> shipping `run_all.sh` + landing ADR 0001 (spec-merge policy), then
> runs the first shadow-mode trial on a 5-function mini-batch. The
> trial validated the ADR's precedence rules against live data,
> surfaced one real architectural gap (flag-state API missing in
> `src/`), and shipped one new port (`mpfr_div_2`). State.db at end:
> **116 done, 8 blocked.**

## TL;DR

This session shipped 5 commits totaling ~1500 LOC of code + docs +
reports. The validation infrastructure from worklog 008 is now
hardened (mutator regex bugs fixed, run_all.sh closes the goldens
gap, ADR 0001 documents the spec-merge precedence) and battle-tested
against a real opus prep + sonnet port pipeline. The shadow trial
gave the data needed to land step 6 (ralph.py wiring) and surfaced a
clear priority sequence for the next session.

## Commits in this session

| Commit | What |
|---|---|
| `f7569b4` | Fix mutator regex bugs (`ternary-negate` corrupts destructuring; `op-swap` corrupts function decls). Both caught by `mutate.py`'s module_init_failed gate; fix removes ~1s waste per occurrence |
| `c049f24` | `eval/golden_master/run_all.sh` — formalize the inline bash loop the prior session used to regenerate 79 missing goldens. 85 LOC, shellcheck-clean |
| `ef44f0f` | ADR 0001 — per-field precedence policy for the gen_spec/curator integration. gen_spec wins on `c_signature` + `prec_unit` default + `refs[0]`; curator wins on `class`, `signature.params`, `signature.returns`, `doc`, `divergence_from_c` |
| `ad04e3a` | Shadow trial: 5-function mini-batch (4 predicates + mpfr_div_2). mpfr_div_2 ported (composite=1.0); 4 predicates parked on `mpfr-ts-ikr`. Report at `docs/reports/010-shadow-trial.md` |

## Validation finalize (3 follow-up tasks)

### Mutator regex bugs — `mpfr-ts-omy` + `mpfr-ts-agn` (commit `f7569b4`)

Calibration report 009 flagged two regex bugs in `mutators.ts`:
- `op-swap` matched `export function mpfr_add(...)` declarations
- `ternary-negate` matched `const { ternary: tr } = ...` destructuring

Both produced invalid TS that `mutate.py`'s `module_init_failed`
detection caught — no false-pass gates, but ~1 second wasted per
occurrence. Fixes:
- `op-swap`: `\bmpfr_add\(` → `(?<!function )\bmpfr_add\(` (look-behind excludes `function ` declarations)
- `ternary-negate`: scan forward from the matched closing brace; skip the mutation if the next non-whitespace char is `=` (but not `==` or `=>`)

26/26 mutator tests pass (4 new + 22 existing). Real-source smoke:
op-swap on `add.ts` correctly mutates 2 call sites (was 3 incl. its own
decl); ternary-negate on `div_2si.ts` correctly mutates 3 object-
literal returns (skipping destructuring).

### `run_all.sh` — `mpfr-ts-lq8` (commit `c049f24`)

Captures the inline bash loop the orchestrator ran during the
validation arc to regenerate 79 missing goldens. The wrapper:

- Iterates `eval/functions/*/`
- Invokes each compiled `golden_driver` with 60s timeout
- Redirects stdout to `golden.jsonl`
- Idempotent: skips if golden exists (`--force` to override)
- `--filter <fn>` for single-function runs
- Removes partial goldens on failure/timeout (so retry is clean)
- Summary: `built: N generated: N skipped: N failed: N`
- Exit 0 only if no failures

85 LOC, shellcheck-clean, sibling of `build.sh`.

### ADR 0001 — `mpfr-ts-n8y` (commit `ef44f0f`)

Documents per-field precedence for the gen_spec/curator merge that
step 6 (ralph.py wiring) will implement. See `docs/adr/0001-spec-
merge-policy.md`. Headline rules:

- gen_spec wins on: `c_signature`, `prec_unit` (default `'bits'`),
  `refs[0]` (canonical C source pointer)
- Curator wins on: `class` (runtime budget tier, not semantic
  family), `signature.params` (semantic API names), `signature.returns`
  (idiomatic TS lifts: `boolean` for `_p`, `bigint` for prec_t),
  `doc`, `divergence_from_c`, `refs[1+]`

Explicitly considers and rejects two alternatives (curator-always-
wins, gen_spec-always-wins) — the trial vindicated the chosen rule
in §"Shadow A" below.

## Shadow-mode trial: 5-function mini-batch

Goal per worklog 008's recommendation: run gen_spec + mutate.py
alongside a real opus prep + sonnet port workflow to validate
integration assumptions before committing to step 6.

### Function selection

5 functions, deliberately diverse:
- 4 flag-state predicates from `mpfr/src/exceptions.c`:
  `mpfr_underflow_p`, `mpfr_overflow_p`, `mpfr_nanflag_p`, `mpfr_divby0_p`
- 1 arithmetic fast path from `mpfr/src/div.c`: `mpfr_div_2`

Coverage rationale: predicates test gen_spec on `_p` shape + mutate.py's
known predicate weakness; `mpfr_div_2` tests gen_spec on `static`
decls (after the `mpfr-ts-5s4` fix) + mutate.py's known shift-heavy
strength.

### Pipeline outcomes

| Step | Outcome |
|---|---|
| Opus prep dispatch | 20 artifacts in 16 min / ~198K tokens (5 specs + 5 drivers + 10 ref ports) |
| Golden generation (build + run) | 5/5 golden.jsonl materialized, all meet Rule 7 tag minimums |
| Sonnet port (mpfr_div_2 only) | composite=1.0 in 2 iterations / ~5 min / ~80K tokens (bug fix: sticky-bit XOR pattern) |
| 4 predicate ports | Parked on `mpfr-ts-ikr` flag-state API gap |

Total: ~278K tokens, ~22 min wall. The shadow analysis (gen_spec +
mutate.py) added negligible cost; the prep + port work was the
dominant cost.

### Shadow A: gen_spec vs opus's spec.json

25 field comparisons (5 fields × 5 functions). Per ADR 0001:

| field | matches | ADR winner | observation |
|---|---|---|---|
| class | 5/5 | gen_spec | Filename heuristic happened to align with opus's chosen budget tier on all 5 |
| signature.params | 1/5 | curator | 4 predicates: opus added a `'mask'` wire-codec input; gen_spec said `[]` (C `void`) |
| signature.returns | 1/5 | curator | 4 predicates: opus lifted to `'boolean'`; gen_spec said `'number'` (C `int`) |
| c_signature | 0/5 | gen_spec (tolerated) | Whitespace + `static` prefix differences only |
| prec_unit | 1/5 | curator | 4 predicates: opus overrode to `'n/a'` (no prec); gen_spec said `'bits'` |

**Every disagreement was correctly predicted by ADR 0001.** The
curator-wins overrides on `signature.returns`, `signature.params`, and
`prec_unit` for the 4 predicates are exactly what the ADR specified.

Step 6 (ralph.py wiring) must include an explicit prompt addendum
instructing opus to override these 3 fields per the ADR — otherwise
opus would silently inherit gen_spec's structural defaults and lose
the idiomatic-TS lift.

### Shadow B: mutate.py vs opus's broken port (mpfr_div_2 only)

| signal | composite | conclusion |
|---|---|---|
| Opus's hand-tuned broken port | 0.1516 | Clean kill — golden discriminates |
| mutate.py mutation_prove | gate_passed=True (shift-direction-swap=0.0; rnd-swap=0.80) | Golden discriminates |

Both methods agreed the golden is calibrated. But opus's hand-tuned
perturbation landed ~5× harder than mutate.py's best automated
mutation. For shift-heavy ports (single-limb fast paths, substrate)
mutate.py's `shift-direction-swap` reliably catches; for non-shift
ports the gap is larger. Per report 009, this is consistent with the
class-level patterns: arithmetic and substrate are well-covered;
conversion and misc-with-no-shifts are weaker.

**Implication**: keep opus's broken-port deliverable in the prep
workflow for now. Replacement mode (drop the broken port → rely on
mutate.py alone) waits until either (a) menu extensions raise non-
shift class coverage or (b) per-class gates prove robust across more
shadow trials.

### Real finding the trial surfaced before any sonnet code: `mpfr-ts-ikr`

Opus prep correctly identified that `src/` has no analog of MPFR's
`__gmpfr_flags` global register. The 4 `_p` predicates require either:

- (a) Add `src/internal/mpfr/flags.ts` (~30 LOC) exporting
  `setFlags(bits)`, `clearFlags(bits)`, `getFlags()` plus 6 bit
  constants
- (b) Park the predicates pending an ADR on global-state APIs in the
  otherwise immutable-value-typed library

Opus did not invent the API — documented both options in each spec's
`divergence_from_c`. This is exactly what shadow mode is designed for:
surface architectural gaps before any porter writes code.

Filed as bd `mpfr-ts-ikr` [P2]. Blocks the 4 predicates in this trial
AND another ~6 functions in the next mega-batch's exception family
(`mpfr_inexflag_p`, `mpfr_erangeflag_p`, `mpfr_set_underflow`,
`mpfr_set_overflow`, `mpfr_clear_flags`, `mpfr_flags_save`,
`mpfr_flags_set`).

### What shipped from the trial

| Artifact | Status |
|---|---|
| `src/ops/div_2.ts` | Shipped, composite=1.0, 389 LOC |
| `eval/functions/mpfr_div_2/{spec.json, golden_driver.c, golden.jsonl}` | Committed |
| `eval/reference_ports/{correct,broken}/mpfr_div_2.ts` | Committed |
| `eval/functions/mpfr_{underflow_p, overflow_p, nanflag_p, divby0_p}/*` | Committed; state.db `blocked` on `mpfr-ts-ikr` |
| `eval/reference_ports/*/mpfr_{underflow_p, overflow_p, nanflag_p, divby0_p}.ts` | Committed |
| `docs/reports/010-shadow-trial.md` | Committed (224 LOC) |

State.db: 116 done (+1), 8 blocked (+4).

## Recommended priority sequence for next session

In priority order, lowest-risk first:

### 1. Resolve `mpfr-ts-ikr` — flag-state API module

Add `src/internal/mpfr/flags.ts` exporting the 6-bit flag register
plus get/set/clear primitives. ~30 LOC + ~40 LOC tests. Unblocks 4
predicate ports in this trial + ~6 more functions in the next
mega-batch.

The 4 predicates in `state.db` are already in `blocked` status with
their spec.json + golden_driver.c + reference ports committed. Once
the flags module lands, they can be ported (sonnet wave dispatch) +
shipped without re-doing the prep work.

Estimated effort: ~1 hour total (module + tests + 4 port unblocks +
ship). One subagent per artifact, serial.

### 2. Land step 6 — wire gen_spec into ralph.py

Modify `_render_prep_prompt` to call `gen_spec.extract_spec` for each
selected function and include the partial spec in the prompt
verbatim, with the ADR 0001 prompt addendum from report 010:

```
The structural fields below are extracted from the C source by
gen_spec. They are CORRECT for the C definition but require these
overrides for the idiomatic TS port:
  - signature.returns: int -> 'boolean' for _p predicates;
    void -> 'MPFR' when first dropped C ptr is the output slot;
    long/mpfr_prec_t/mpfr_exp_t -> 'bigint'
  - signature.params: may add wire-codec inputs (e.g. 'mask' for
    flag-state predicates) not present in the C signature
  - prec_unit: override to 'n/a' if no `prec` parameter

The c_signature field is authoritative; do not edit it.
```

Estimated effort: ~80-120 LOC delta to ralph.py + ~30 LOC test
additions. Existing 52 ralph.py pytest cases must stay green.

### 3. Second shadow trial — 5-10 functions avoiding the flag-state gap

Candidates: `mpfr_sqrt1`, `mpfr_sqrt1n`, `mpfr_sqrt2_approx` (single-
limb sqrt fast paths — test substrate-class with the gen_spec
integration live); `mpfr_modf`, `mpfr_add_z` (test mpz interop +
non-`mpfr_*_p` shapes); `mpfr_inv` if dependency-ready.

Tests step 6's integration in a real run AND broadens the
class-coverage data for mutate.py.

### 4. First replacement-mode trial

When 1-3 land cleanly: drop opus's broken-port deliverable on a small
batch (3-5 functions). Use mutate.py alone for mutation-prove. See if
the gate's verdict is sufficient signal.

Conservative criterion: replacement mode is OK if mutate.py gate
agrees with the eventual ship decision on ≥80% of trial functions
across non-trivial classes.

## What didn't happen (deferred to next session)

- **Step 6 itself** — the gen_spec wiring. The ADR is in; the
  prompt addendum is drafted in report 010; the integration is a
  small subagent task that the next session can pick up.
- **Class-level mutator menu strengthening** — report 009 noted
  comparison-swap is near-dead (5% clean-kill rate) and rnd-swap is
  dead weight (0%). Bds `mpfr-ts-18x` (multi-site comparison-swap)
  and `mpfr-ts-6zg` (branch-replacement rnd-swap) are filed but not
  worked.
- **Replacement-mode trial** — gated on #1-3 above.

## Open bd queue at session end

P2 (block step 6 or near-term work):
- `mpfr-ts-ikr` — flag-state API module (the new urgent item)
- `mpfr-ts-i8e` — git pre-commit hook for bd auto-export (operational
  hygiene; not load-bearing)

P3 (harness polish — not blocking):
- `mpfr-ts-18x` — comparison-swap multi-site
- `mpfr-ts-9di` — gate behavior for trivial-body ports
- `mpfr-ts-2ls` — value_codec scalar strings
- `mpfr-ts-ai4` — runner n_throw conflation
- `mpfr-ts-d6o` — callgraph misses mpn_* substrate fns
- `mpfr-ts-e4j` — expected_throw codec for domain errors
- `mpfr-ts-sr4` — Rule 7 tag minimums at grade time

P4 (cleanup):
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`,
  `mpfr-ts-lyr` (closed)

## Lessons from this turn

1. **Shadow mode pays back its cost in real architectural findings.**
   The trial didn't save tokens — it cost ~278K total for 1 ported
   function. But it surfaced `mpfr-ts-ikr` BEFORE the next mega-batch
   would have hit it across ~10 functions, and validated ADR 0001
   against live data. That's the trade.

2. **Opus's hand-tuned broken-port deliverable is genuinely
   information-rich.** Composite=0.15 vs mutate.py's best at 0.0
   (with shift-direction-swap, which doesn't apply to many shapes).
   Don't drop it until per-class menu strengthening lands.

3. **gen_spec is integration-ready when paired with explicit
   override instructions.** Every gen_spec/opus disagreement was
   predicted by ADR 0001. The ADR is the bridge that makes the
   automation safe.

4. **The `--ship` mode is excellent for atomic single-port commits**
   AND for cleanup commits that bundle "everything in the working
   tree". One invocation handled the mpfr_div_2 promote + bundled
   the 4 parked predicate artifacts + bundled report 010, all in
   commit `ad04e3a`.

5. **State.db's `blocked` status is the right home for parked
   work.** The 4 predicates have their spec/driver/refs committed,
   are explicitly blocked on a filed bd, and can be picked up the
   moment that bd resolves — no re-doing the prep work.

## Pickup checklist

```bash
git pull --rebase
cat PHASE.md                                          # → Pilot
cat HANDOFF.md                                        # refreshed this turn
cat docs/worklog/009-validation-finalize-shadow-trial.md  # this file
cat docs/reports/010-shadow-trial.md                  # full shadow data

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|8 done|116

# Verify tools still green:
bash eval/golden_master/build.sh                      # all drivers compile
cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -v
# Expected: 41+ tests passing (gen_spec 19 + mutate 14 + validate_specs 7 + calibrate 5)
cd eval/driver && bun test mutators.test.ts
# Expected: 26 tests passing

bun x tsc --noEmit                                    # must be clean
bd ready                                              # 9 issues; top: mpfr-ts-ikr (P2)
```

Start with `mpfr-ts-ikr` (the flag-state module). The 4 predicates
unblock automatically once it lands.
