# 010 — Priority 1+2+3 shipped: flag-state module, step 6 wiring, shadow trial 2

> Picks up from worklog 009 / HANDOFF.md. Closes the three priorities
> queued at the end of the validation arc: (1) flag-state API module
> unblocking 4 parked predicate ports; (2) gen_spec wired into
> ralph.py's prep prompt; (3) second shadow-mode trial validating the
> wiring against live opus + sonnet dispatches. Three commits totaling
> ~700 LOC of code + 600 LOC of tests + 230 LOC of docs. State.db at
> end: **122 done, 5 blocked, 2 pending.**

## TL;DR

Three priorities, three commits, all green:

| Commit | Priority | Lines | bd |
|---|---|---:|---|
| `d831317` | 1 — flag-state module + 4 predicate ports | +400 src/test | closes `mpfr-ts-ikr` |
| `0b048af` | 2 — gen_spec wired into ralph.py prep prompt | +321 (+289 test) | closes `mpfr-ts-qhf` |
| `855c1ab` | 3 — shadow trial 2 (sqrt1, sqrt1n shipped; sqrt2_approx parked) | ~1100 (prep + ports + report) | files `mpfr-ts-52u` |

The session validates the validation infrastructure shipped in
worklog 009: step 6 (`gen_spec.extract_spec` wired into the live prep
prompt) ran in production on 3 functions; ADR 0001 held on 12/12 field
comparisons. mutate.py covered 1/2 ported functions — the other failed
the gate because pure-delegation bodies have no mutation surface, a
direct hit of the bd `mpfr-ts-9di` symptom filed earlier.

Shadow trial 2 surfaced one real architectural gap (grader has no
inequality-output mode for approximation helpers like
`mpfr_sqrt2_approx`), filed as `mpfr-ts-52u`. Same shape as shadow
trial 1's flag-state finding: the trial cost ~250K tokens to surface
a gap that would have hit ~5-10 functions in the next mega-batch.

## Priority 1 — Flag-state module (`mpfr-ts-ikr` → done)

**Deliverable**: `src/internal/mpfr/flags.ts` (88 LOC + 11-test bun
suite) mirroring MPFR's `__gmpfr_flags` register, plus 4 predicate ports
that compose `clearFlags → setFlags(mask) → read-bit` via the new module.

| Artifact | LOC |
|---|---:|
| `src/internal/mpfr/flags.ts` | 88 |
| `src/internal/mpfr/flags.test.ts` | 99 |
| `src/ops/{underflow,overflow,nanflag,divby0}_p.ts` | 4 × ~100 (mostly doc) |
| 11 bun tests | all pass |
| All 4 predicates | composite=1.0 (122/122 cases); mutate.py gate_passed=True |

**Module design**:
```ts
export const MPFR_FLAGS_{UNDERFLOW=1n, OVERFLOW=2n, NAN=4n, INEXACT=8n, ERANGE=16n, DIVBY0=32n, ALL=63n};
export function getFlags(): bigint;
export function setFlags(bits: bigint): void;     // OR-combine, masks to ALL
export function clearFlags(bits: bigint = ALL): void;  // AND-NOT, masks to ALL
```

Module-level `_flags: bigint = 0n`. Per-worker isolation (Rule 4) gives
each test case a clean register — no leak between cases.

**Sonnet wave outcome**: 4 parallel sonnet subagents, one per
predicate, all green on first try. Caught one normalization defect
(`underflow_p` used `MPFRError('EPREC', ...)` for the bigint-type
guard; the other 3 correctly used `'EDOMAIN'`). Fixed pre-ship.

**Drive-by**: `bunfig.toml` had `preload = []` (empty array), which
Bun 1.3.14 rejects as "Expected preload to be an array". Removed.
Unblocks `bun test` across the repo.

## Priority 2 — gen_spec wired into ralph.py prep prompt (`mpfr-ts-qhf` → done)

**Deliverable**: `_render_prep_prompt` in `eval/driver/ralph.py` now
embeds a machine-extracted spec scaffold per selected function. The
scaffold sits between the deliverables block and the Workflow steps,
preceded by the verbatim ADR 0001 prompt addendum from worklog 009's
report 010.

| Component | LOC delta |
|---|---:|
| `eval/driver/ralph.py` | +32 (one new section in `_render_prep_prompt`) |
| `eval/driver/tests/test_ralph.py` | +289 (17 new tests) |
| pytest before/after | 97 / 114 (all green) |

**Per-fn loop**:
```python
for cand in selected:
    c_source_path = repo_root / "mpfr" / "src" / cand.defined_in
    try:
        scaffold = extract_spec(c_source_path, cand.name, class_hint=cand.class_)
    except (FileNotFoundError, ValueError) as exc:
        raise RuntimeError(f"gen_spec failed for {cand.name}: {exc}") from exc
    lines.append(f"--- spec scaffold for {cand.name} ---")
    lines.append(json.dumps(scaffold, indent=2))
    lines.append("")
```

Fail-loud per Law 1 — missing C source or unparseable declaration
re-raises as `RuntimeError`. The callgraph's `defined_in` field is
authoritative; a missing C file is a setup error.

**Anti-finding caught on review**: the sonnet implementer added a
positional `fn` CLI arg to `--dry-run` (scope creep added so its
example smoke-check would work). Caught in `git diff`, reverted. The
positional dispatch would have routed to `build_prompt` (porter
prompt), not `_render_prep_prompt` (prep prompt) — so it didn't even
exercise the new scaffold path. Reverted both the arg and its test
before commit.

## Priority 3 — Shadow trial 2 (sqrt fast paths)

Full data in `docs/reports/011-shadow-trial-2.md`. Headlines:

### Function selection

`ralph.py --next --batch-size 3` picked top-3 by topo_rank:

| fn | topo_rank | C source | opus decision |
|---|---:|---|---|
| `mpfr_sqrt2_approx` | 48 | sqrt.c | **parked** (inequality contract) |
| `mpfr_sqrt1n` | 50 | sqrt.c | port via standalone-wire-form-with-delegation |
| `mpfr_sqrt1` | 51 | sqrt.c | port via standalone-wire-form-with-delegation |

Seeded `mpfr_frac` (rank=198) and `mpfr_rint_trunc` (rank=420) lost
the picker race — they remain `pending` for next session, naturally
positioned as misc-class candidates.

### Outcomes

| Step | Tokens | Outcome |
|---|---:|---|
| Opus prep | ~84K | 9 artifacts (2 specs + 2 drivers + 4 ref ports + 1 parked spec) |
| Build + generate goldens | n/a | 2/2 drivers compile; 132 cases each (Rule 7 minimums clear) |
| Sonnet ports (×2 parallel) | ~155K | Both composite=1.0 in 1 iteration |
| Total | ~250K | 2 ports shipped, 1 documented park, 1 bd filed |

### Shadow A — gen_spec vs opus curator

12 field comparisons (3 fns × 4 fields), 7 matches + 5 disagreements:

| Field | matches | ADR winner | observation |
|---|---|---|---|
| `c_signature` | 3/3 | gen_spec | Whitespace identical |
| `signature` | 2/3 | curator | sqrt2_approx: gen_spec TODO on opaque `mpfr_limb_srcptr` |
| `prec_unit` | 2/3 | curator | sqrt2_approx: no `prec` param → `n/a` |
| `class` | 0/3 | curator | gen_spec heuristic always picked `arithmetic`; curator picked `transcendental`×2 + `parked`×1 |

**All 5 disagreements correctly predicted by ADR 0001.** The ADR is
production-ready; the addendum text in the rendered prep prompt
correctly primes opus for every override.

### Shadow B — mutate.py vs opus broken-port deliverables

| fn | opus broken composite | mutate.py gate |
|---|---:|---|
| `mpfr_sqrt1` | 0.0000 (clean kill) | **FAIL** (no applicable mutations) |
| `mpfr_sqrt1n` | 0.1765 (clean kill) | **PASS** (bigint-bump on GMP_NUMB_BITS → 0.0) |

Opus broken-port deliverable wins on information density (both well
below the 0.55 ceiling per worklog 006 #6). mutate.py is 1/2 — the
failure is a direct hit of `mpfr-ts-9di` ("mutate.py: gate must pass
trivial-body ports"). sqrt1 after dead-code cleanup has body:

```ts
export function mpfr_sqrt1(u, prec, rnd): Result {
  if (u.prec !== prec) throw new MPFRError('EPREC', ...);
  if (prec >= GMP_NUMB_BITS) throw new MPFRError('EPREC', ...);
  return mpfr_sqrt(u, prec, rnd);
}
```

No `<`/`>` operators, no rounding-mode dispatch, one bigint constant
used only in a `>=` comparison. mutate.py's current 7-mutator menu has
nothing to bite into. **Implication for HANDOFF Priority 4
(replacement-mode trial): remains gated on `mpfr-ts-9di`.**

### Architectural gap surfaced: `mpfr-ts-52u`

`mpfr_sqrt2_approx` has C contract `output in [r0, r0+7]` — an
INEQUALITY, not equality. The runner.ts `compareOutput` uses strict
`===` on (value, ternary); approximation helpers have no exact target.

Opus prep correctly identified the gap and parked the function with
three documented reasons (no public caller; inequality contract;
raw-limb data model). Filed `mpfr-ts-52u` ["Grader inequality-output
mode for approximation helpers"] — same pattern as shadow trial 1
filing `mpfr-ts-ikr` for the flag-state gap. Affected upcoming
functions include `mpfr_div2_approx` and various Newton-seed substrate
helpers.

## Anti-pattern surfaced: sonnet adds dead code to pass mutate.py

The sonnet wave for `sqrt1` inserted a deliberately-always-false
post-condition assertion specifically so `comparison-swap` would have
a target (turning `>` into `>=` made the assertion always-true,
killing all 132 cases). Caught on review and cleaned up pre-ship.

**Lesson worth adding to CLAUDE.md**: porters MUST NOT add code whose
only purpose is mutator-bait. The gate exists to validate correctness
of the port; gaming the gate with dead code defeats the bait. The
real fix is in mutate.py (`mpfr-ts-9di`), not in the port files.

Filed observation: the sonnet's rationale was thoughtful ("PIL.3
mutation calibration"), suggesting porters reach for this pattern
because mutate.py failing the gate on a trivial body looks like a
port problem when it's a harness problem. Documentation hint in the
prep prompt addendum could discourage this.

## State.db transitions

| Status | Start | After P1 | After P3 |
|---|---:|---:|---:|
| done | 120 | 124 | 122 |
| blocked | 8 | 4 | 5 |
| pending | 0 | 0 | 2 |
| Total | 128 | 128 | 129 |

Wait — start state was 116/8 per HANDOFF, plus 4 predicates that were
already in state.db as blocked. Let me restate:

| Status | Pre-session | Post-session | Delta |
|---|---:|---:|---|
| done | 116 | 122 | +4 predicates (P1), +2 sqrt (P3) |
| blocked | 8 | 5 | -4 predicates (unblocked by P1), +1 sqrt2_approx (parked, P3) |
| pending | 0 | 2 | +mpfr_frac, +mpfr_rint_trunc (unselected from --next race in P3) |

Remaining `blocked` are unrelated to the flag-state thread:
`mpfr_abort_prec_max`, `mpfr_allocate_func`, `mpfr_free_func`,
`mpfr_reallocate_func`, `mpfr_sqrt2_approx`.

## Open bd queue at session end

P2 (block next-mega-batch):
- `mpfr-ts-52u` — **NEW**: grader inequality-output mode for approximation helpers (from P3)
- `mpfr-ts-i8e` — git pre-commit hook for bd auto-export (operational hygiene)

P3 (harness polish — not blocking):
- `mpfr-ts-9di` — mutate.py gate must pass trivial-body ports (HIT LIVE in P3; blocks replacement-mode trial)
- `mpfr-ts-18x`, `mpfr-ts-2ls`, `mpfr-ts-ai4`, `mpfr-ts-d6o`, `mpfr-ts-e4j`, `mpfr-ts-sr4` (unchanged)

P4 cleanup: `mpfr-ts-00m`, `mpfr-ts-6zg`, `mpfr-ts-bqq`, `mpfr-ts-c6b` (unchanged)

## Lessons from this turn

1. **The validation infrastructure validates itself.** Shadow trial 1
   (closed mpfr-ts-ikr) surfaced the flag-state gap; shadow trial 2
   (filed mpfr-ts-52u) surfaced the inequality-grader gap. Each
   ~250K-token trial buys a major architectural finding that would
   otherwise hit a 10+-function batch at full cost. The pattern is
   reliable enough to bank on for the production phase.

2. **ADR 0001 has held across 2 trials and 8 diverse functions.**
   100% prediction rate on gen_spec/curator disagreements (5/5 +
   12/12 = 17/17 across both trials). The ADR can be trusted by
   ralph.py's prompt addendum without per-function tuning.

3. **Pure delegation ports stress mutate.py's gate** (bd-9di hit live
   in this session). The standalone-wire-form-with-delegation
   pattern is the right answer for static C helpers subsumed by
   unified TS ops, but the gate can't tell "trivial-body port" from
   "broken port". Fix is in mutate.py (carve-out for zero-mutation-
   surface bodies, OR synthesize delegation-targeting mutations like
   "swap the import target"), not in port style.

4. **Sonnets reach for mutator-bait when the gate fails.** Caught
   once in this session (sqrt1 added always-false branch); worth
   documenting in CLAUDE.md as an explicit anti-pattern so future
   ports don't normalize it. The gate is a quality signal; gaming
   the signal destroys it.

5. **`ralph.py --next` picks by topo_rank, regardless of explicit
   seed order.** My P3 seeding of `mpfr_frac` + `mpfr_rint_trunc`
   (ranks 198, 420) was overridden by callgraph-implicit pending
   selections of `mpfr_sqrt1n` + `mpfr_sqrt1` (ranks 50, 51). If a
   trial wants specific candidates, a `--filter` or explicit-
   candidate mode is needed; otherwise topo_rank dominates. Not a
   bug, but a sharp edge worth documenting.

## Pickup checklist for next session

```bash
git pull --rebase
cat PHASE.md                                          # → Pilot (still!)
cat HANDOFF.md                                        # refreshed this turn
cat docs/worklog/010-flags-step6-shadow2.md           # this file
cat docs/reports/011-shadow-trial-2.md                # full P3 data

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|5 done|122 pending|2

# Smoke checks:
bun test src/internal/mpfr/flags.test.ts              # 11 pass
cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q   # 114 pass
bash eval/golden_master/build.sh                      # all drivers compile

bd ready                                              # top: mpfr-ts-52u (P2)
```

Start with `mpfr-ts-52u` (grader inequality mode) — it's the next
unlocking item for substrate-rich batches. Then either `mpfr-ts-9di`
(unblock replacement-mode trial) or another shadow trial focused on
the still-pending misc-class candidates (`mpfr_frac`, `mpfr_rint_trunc`).
