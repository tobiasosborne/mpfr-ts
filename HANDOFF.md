# Handoff — 116 ports + validation infra landed; next: flag-state module → step 6 → next shadow

You are picking up mpfr-ts after a session that shipped the opus-prep
automation infrastructure end-to-end (5 tools + 3 reports + 1 ADR)
and ran the first shadow-mode trial on a 5-function mini-batch. The
trial validated the integration assumptions, surfaced one real
architectural gap, and shipped 1 new port. State.db: **116 done, 8
blocked** (4 newly blocked on the surfaced gap).

The validation arc is complete. The next session does **integration
work**, not more validation.

## ⚠ Three gotchas — read first

1. **`.gitignore` `mpfr/` pattern.** Anchored to `/mpfr/` since `cb65ebe`. If you add a directory whose name collides with an ignored pattern, audit the gitignore. Last bite: ~12 hours of silently-dropped substrate files in the 50→85 session.

2. **`bd` commands don't auto-export to JSONL.** `ralph.py --commit-batch` and `--ship` do this automatically. Manual `git commit` skips it. **Always run `bd export -o .beads/issues.jsonl` before manual commits**, or prefer `--ship`. `mpfr-ts-i8e` tracks the pre-commit-hook fix.

3. **`PHASE.md` still says `Pilot`** despite 116 ports. Per CLAUDE.md, Pilot was the *first 10*. Rule 14 requires a transition worklog before flipping the file. This has been deferred across multiple sessions because the orchestrator has been running "Pilot+" (halt-on-failure, no opus escalation) successfully. Surfaced here for awareness; not blocking near-term work.

## TL;DR — first 10 minutes

```bash
git pull --rebase
cat PHASE.md                                          # → Pilot
cat HANDOFF.md                                        # this file
cat docs/worklog/009-validation-finalize-shadow-trial.md  # latest session
cat docs/worklog/008-automation-infra.md              # validation arc context
cat docs/reports/010-shadow-trial.md                  # shadow-trial findings
cat docs/adr/0001-spec-merge-policy.md                # integration policy

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|8 done|116

# Verify tools green:
bun x tsc --noEmit                                    # must be clean
cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -v
# Expected: 41+ passing across gen_spec, mutate, validate_specs, calibrate
cd eval/driver && bun test mutators.test.ts
# Expected: 26 passing

bd ready                                              # 9 P2-P4 issues
```

## Next-session priority sequence

The shadow trial gave clean signal on what to do next. Work in this order:

### Priority 1: Resolve `mpfr-ts-ikr` (P2) — flag-state API module

The shadow trial surfaced that `src/` has no analog of MPFR's `__gmpfr_flags` global register. This blocks 4 functions already parked in this session (`mpfr_underflow_p`, `mpfr_overflow_p`, `mpfr_nanflag_p`, `mpfr_divby0_p`) AND another ~6 functions in the upcoming exception family (`mpfr_inexflag_p`, `mpfr_erangeflag_p`, `mpfr_set_underflow`, `mpfr_set_overflow`, `mpfr_clear_flags`, `mpfr_flags_save`, `mpfr_flags_set`).

**Deliverable**: `src/internal/mpfr/flags.ts` (~30 LOC) exporting:
- Bit constants: `UNDERFLOW=1n, OVERFLOW=2n, NAN=4n, INEXACT=8n, ERANGE=16n, DIVBY0=32n, ALL=63n`
- `getFlags(): bigint` — read the register
- `setFlags(bits: bigint): void` — OR-combine into register
- `clearFlags(bits: bigint = ALL): void` — AND-NOT out of register

Plus tests verifying get/set/clear semantics + bit-mask combinations.

After the module lands, the 4 parked predicate ports can be unparked: their spec.json + golden_driver.c + reference ports are already committed (see commit `ad04e3a`), so dispatch sonnet ports referencing the new flags module. Each port is ~5-10 LOC.

Estimated effort: ~1 hour (module + tests + 4 sonnet port dispatches + ship).

### Priority 2: Land step 6 — wire gen_spec into ralph.py's prep prompt

The validation arc (gen_spec, mutate.py, validate_specs, calibrate) built and tested all the tools. The shadow trial validated ADR 0001's precedence rules. Now do the actual integration.

**Deliverable**: modify `eval/driver/ralph.py`'s `_render_prep_prompt` to:
1. Call `gen_spec.extract_spec` for each selected function (look up C source via `callgraph.json`'s `defined_in` field)
2. Include the partial spec.json scaffold in the prompt
3. Append the ADR-derived prompt addendum (see `docs/reports/010-shadow-trial.md` §Recommendations for the verbatim text):

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

**Constraints**: All 52 existing `ralph.py` pytest cases must stay green. Add ~20 new tests covering the gen_spec integration (input → expected rendered prompt fragment for a few function shapes).

Estimated effort: ~80-120 LOC delta + ~30 LOC tests. ~2 hours.

### Priority 3: Second shadow trial — 5-10 functions avoiding the flag-state gap

Validate step 6's integration in a real run AND broaden class-coverage data for mutate.py.

**Candidates**:
- Single-limb sqrt fast paths: `mpfr_sqrt1`, `mpfr_sqrt1n`, `mpfr_sqrt2_approx` (substrate-class; test shift-direction-swap dominance from report 009)
- Mpz interop: `mpfr_add_z`, `mpfr_set_z` (test gen_spec on non-const `mpz_ptr` types — the `mpfr-ts-eqc` fix should now handle these)
- Modular ops: `mpfr_modf` (its deps `mpfr_frac` + `mpfr_rint_trunc` are unported but callgraph can sequence them)

Same shape as the first shadow trial: opus prep + sonnet wave + parallel gen_spec/mutate.py analysis. Write findings to `docs/reports/011-shadow-trial-2.md`.

Estimated cost: ~300-400K tokens. Same shadow-mode pattern, larger sample.

### Priority 4: First replacement-mode trial

Gated on 1-3 above. Drop opus's broken-port deliverable on a 3-5 function batch; rely on mutate.py alone for mutation-prove. See if the gate's verdict is sufficient signal.

**Conservative criterion**: replacement mode is OK if mutate.py gate agrees with the eventual ship decision on ≥80% of trial functions across non-trivial classes. If less, defer until mutator menu is strengthened.

## What's working now (don't change)

| Component | Path | Notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen; never modify without ADR |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Solid |
| AST gate | `eval/harness/ast_check.ts` | Solid |
| Substrate | `src/internal/{mpn,mpfr}/` | 18 files |
| Callgraph | `eval/driver/callgraph.py` | 525 fns; re-run if you touch `mpfr/src/` |
| State DB | `eval/state.db` | 124 rows; 116 done, 8 blocked |
| **NEW**: gen_spec | `eval/driver/gen_spec.py` | 207 LOC; structural extraction from C; bugs all fixed |
| **NEW**: mutators | `eval/driver/mutators.ts` | 185 LOC; 7 mutations; calibrated 79% gate pass |
| **NEW**: mutate orchestrator | `eval/driver/mutate.py` | 242 LOC; import-rewrite + module-init-failed detection |
| **NEW**: validate_specs | `eval/driver/validate_specs.py` | 167 LOC; gen_spec vs curator diff tool |
| **NEW**: calibrate | `eval/driver/calibrate.py` | 149 LOC; mutate.py runner across stratified samples |
| **NEW**: run_all.sh | `eval/golden_master/run_all.sh` | 85 LOC; materialize golden.jsonl across all built drivers |
| **NEW**: ADR 0001 | `docs/adr/0001-spec-merge-policy.md` | spec-merge precedence rules |

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR
- Flip `PHASE.md` from `Pilot` to `Production` without writing `docs/worklog/NNN-phase-transition.md` first (CLAUDE.md Rule 14)
- Disable harness gates to make a port pass. Fix the port instead
- Skip mutation-prove (broken < 0.55 ideally < 0.30 per worklog 006 #6)
- Re-introduce the absolute-path import bug (handled by `_promote_port`)
- Dispatch all N sonnets simultaneously when N > 10. Waves of 6-10 remain the cost-disciplined default
- Drop opus's broken-port deliverable BEFORE replacement-mode trial validates the gate (priority 4). The current data justifies keeping it
- Modify the shipped infrastructure tools (gen_spec, mutators, mutate, validate_specs, calibrate) without bd-driven justification — they're validated and load-bearing

## Pickup-on-different-device checklist

1. `git clone git@github.com:tobiasosborne/mpfr-ts.git`
2. `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. `curl -fsSL https://bun.sh/install | bash`
4. `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. `pip install pytest`
6. `bd bootstrap --yes && bd hooks install && bd import`
7. Smoke-check:
   - `bun x tsc --noEmit`
   - `bash eval/golden_master/build.sh`  # all drivers compile
   - `cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -v`  # 41+ pass
   - `cd eval/driver && bun test mutators.test.ts`  # 26 pass
8. Read CLAUDE.md → this file → `docs/worklog/009-validation-finalize-shadow-trial.md` → `docs/reports/010-shadow-trial.md` → `docs/adr/0001-spec-merge-policy.md`

## Open bd issues at session end (9 total)

P2 — block near-term work:
- `mpfr-ts-ikr` — flag-state API module (priority 1 above)
- `mpfr-ts-i8e` — git pre-commit hook for bd auto-export

P3 — harness polish (not blocking):
- `mpfr-ts-18x` — comparison-swap multi-site
- `mpfr-ts-9di` — mutate.py gate behavior for trivial-body ports
- `mpfr-ts-2ls` — value_codec scalar strings
- `mpfr-ts-ai4` — runner n_throw conflation
- `mpfr-ts-d6o` — callgraph misses mpn_* substrate fns
- `mpfr-ts-e4j` — expected_throw codec for domain-error goldens
- `mpfr-ts-sr4` — enforce Rule 7 tag minimums at grade time

P4 — cleanup:
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

`bd ready` for the live picture.

## One final thing

The 50→85 push proved the engine works. The 85→115 push proved it
scales. This session proved the **automation infrastructure works
and integrates safely** — the validation arc surfaced and fixed real
silent-wrong bugs (gen_spec call-site garbage on 18 functions;
mutate.py false-positive gate from import resolution); the shadow
trial validated ADR 0001 against live data; the integration is now
gated only on the 4 priority items above.

Priority 1 (flag-state module) is the smallest, most-leverage next
move: ~30 LOC unblocks ~10 functions. Start there.

Good luck.
