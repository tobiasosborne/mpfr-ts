# Handoff — 128 ports, 14 blocked, 2 pending; next: substrate batch unblocks downstream

You are picking up mpfr-ts after a second Production batch that shipped
4 trivial accessor ports (`mpfr_buildopt_bfloat16_p`,
`mpfr_buildopt_decimal_p`, `mpfr_get_emin`, `mpfr_get_emax`), parked 6
static helpers under ADR 0002, and blocked 3 functions pending API
decisions. State.db: **128 done, 14 blocked, 2 pending.**

The session also surfaced two architectural frictions:

- **Rule 7 doesn't fit no-arg ports** — Tag minimums (happy>=20, etc.)
  assume non-empty input domain. The 4 trivial ports ship with 1 case
  each; a carve-out clause needs to go into `mpfr-ts-sr4` before Rule 7
  enforcement lands.
- **AST gate require-core-import on no-arg ports** — Filed as bd
  `mpfr-ts-l4t`. P4. 4 ports currently carry a dead type-only import to
  satisfy the gate.

Cost this session: ~$0.31. Cumulative cost across batches 1+2: ~$1.19.

## ⚠ Three gotchas — read first

1. **Picker output now skews toward parks at higher ranks.** This
   session: 4 ports / 9 parks / 2 defers out of 15 candidates (27% port
   rate). As the ralph loop moves into rank 200+ territory, expect
   more `static` C helpers (mostly transcendental aux funcs) that park
   under ADR 0002 criterion (i). **Triage signatures inline before
   dispatching subagents** — saves 5-10x on subagent cost.

2. **State.db has 2 deferred-pending rows** (`mpfr_nbits_ulong`,
   `mpfr_scale2`). They were intentionally left pending — low value,
   no clear TS consumer. If the next session picker doesn't return
   better candidates, defer them again or recategorize as blocked.

3. **`bd` doesn't auto-export to JSONL on manual commits.** Run
   `bd export -o .beads/issues.jsonl` before `git commit` or use the
   `ralph.py --commit-batch`/`--ship` flow. Tracked by `mpfr-ts-i8e`.

## TL;DR — first 10 minutes

```bash
git pull --rebase
cat PHASE.md                                          # → Production
cat HANDOFF.md                                        # this file
cat docs/worklog/013-second-production-batch.md       # latest session
cat docs/worklog/012-first-production-batch.md        # previous
cat docs/adr/0002-approximation-helper-grading.md     # load-bearing

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|14 done|128 pending|2

cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q   # 119 pass
bash eval/golden_master/build.sh                      # all drivers compile

# Smoke-check this session's ports + carve-out:
for fn in mpfr_buildopt_bfloat16_p mpfr_buildopt_decimal_p mpfr_get_emin mpfr_get_emax; do
  base=${fn#mpfr_}
  bun eval/harness/runner.ts --function $fn --port src/ops/$base.ts \
    --golden eval/functions/$fn/golden.jsonl --output /tmp/v.json
done
python3 eval/driver/mutate.py --function mpfr_buildopt_bfloat16_p \
  --port src/ops/buildopt_bfloat16_p.ts \
  --golden eval/functions/mpfr_buildopt_bfloat16_p/golden.jsonl
# Expected: gate_passed: True (vacuous)

bd ready                                              # 16 issues
```

## Next-session priority sequence

### Priority 1 (recommended): Substrate batch — port mpn_divrem family

The 3 substrate candidates from the callgraph (`mpn_divrem`,
`mpn_divrem_1`, `mpn_tdiv_qr`) live in `mpfr/src/mpfr-mini-gmp.c` and
block ~15-30 downstream functions at low rank: `mpfr_sub1sp` (rank
15), `mpfr_rint` (rank 26), `mpfr_addrsh` (rank 36), `mpfr_sqr_3`
(rank 53), `mpfr_mul_3` (rank 61), `mpfr_nexttozero` (rank 84),
`mpfr_nexttoinf` (rank 85). All currently blocked by missing mpn_*
primitives.

The substrate ports are non-trivial (each is ~50-150 LOC of
multi-precision arithmetic) but unlock high-value downstream work.
Better leverage than another batch of misc accessors.

**Deliverable**: port `mpn_divrem_1` first (simplest of the three,
~50 LOC, divides multi-limb number by single limb). Then `mpn_divrem`
(more general; depends on divrem_1) and `mpn_tdiv_qr` (full division).
Plus the missing primitives the picker eligibility query revealed:
`mpn_add_1`, `mpn_sub_1`, `mpn_rshift`, `mpn_mul_n`, `mpn_sqr`,
`mpn_copyi`, `mpn_zero` (most are small bigint operations).

After this session, expect 15-30 newly-ready functions at rank 15-100.

Estimated cost: ~$3-8 in subagent dispatches. Estimated effort: 2-3
hours including verification.

### Priority 2: Pick up `mpfr-ts-l4t` (AST gate friction)

Small architectural cleanup. Change `eval/harness/ast_check.ts` or
`runner.ts` so that ports without an MPFR-typed parameter are exempt
from `requireCoreImport`. Then strip the dead `import type { MPFR as
_MPFR } from '../core.ts'` from the 4 trivial accessors shipped this
session.

Estimated effort: ~30 minutes.

### Priority 3: Pick up `mpfr-ts-3a9` (mpz/bigint ADR)

ADR-shaped work. Decide whether the TS port exposes `_z` variants of
arithmetic ops (mpfr_add_z, mpfr_sub_z, etc.) taking a `bigint`
argument, or whether users compose via `mpfr_set_from_bigint(z)
-> mpfr_add(x, ..., prec, rnd)`. Outcome unblocks a class of ~10-15
`_z` variants in the callgraph (sweep `mpfr_*_z` patterns).

If choosing `_z` variants: write the ADR, ship `mpfr_add_z` as the
reference port, then sweep the rest. If composing: ship a clean
`mpfr_set_z` helper and recommend that path in user-facing docs.

Estimated effort: 1-3 hours including ADR.

### Priority 4: Re-run picker for more pending

If the substrate batch isn't appealing, re-run `ralph.py --next
--batch-size 15`. Ranks 213+ will surface. Expect similar mix to this
session (mostly parks; a few real ports). Cost-effective if you want
breadth over depth.

### Priority 5: Other open P3/P4 issues (carried from worklog 012)

- `mpfr-ts-9di` — mutate.py option (b)/(c) for applied-but-survived
- `mpfr-ts-i8e` — git pre-commit hook for bd export
- `mpfr-ts-4x5`, `mpfr-ts-e2n` — string-IO + printf ADRs
- harness polish: `mpfr-ts-18x`, `mpfr-ts-2ls`, `mpfr-ts-ai4`,
  `mpfr-ts-d6o`, `mpfr-ts-e4j`, `mpfr-ts-sr4`
- cleanup: `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

None block scale-out.

## What's working now (don't change)

| Component | Path | Notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Strict equality; `requireCoreImport` rule fires on misc/arithmetic, exempts substrate |
| AST gate | `eval/harness/ast_check.ts` | Friction on no-arg ports — bd `mpfr-ts-l4t` |
| Substrate | `src/internal/{mpn,mpfr}/` | 19 files; needs expansion for Priority 1 |
| Callgraph | `eval/driver/callgraph.json` | 525 fns; re-extract before any picker batch |
| State DB | `eval/state.db` | 144 rows; 128 done, 14 blocked, 2 pending |
| gen_spec | `eval/driver/gen_spec.py` | arg order `(c_source_path, function_name)` |
| mutate.py | `eval/driver/mutate.py` | gate_status: killed/vacuous/survived |
| ralph picker | `eval/driver/ralph.py --next` | seed + select; reports SELECTED + PREP-PROMPT |
| ADR 0001, 0002 | `docs/adr/` | Both load-bearing |
| **NEW**: 4 accessor ports | `src/ops/{buildopt_bfloat16_p, buildopt_decimal_p, get_emin, get_emax}.ts` | Composite=1.0; 2 vacuous + 2 killed |

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR. (`mpfr-ts-l4t` cleanup goes through
  `ast_check.ts` and/or `runner.ts`, not core.ts.)
- Modify ADR 0001 or 0002 without writing a successor.
- Skip triage. The picker's higher-rank output is parking-heavy; dispatching
  a subagent per candidate burns 5-10x. Inspect signatures inline first.
- Add dead-code workarounds without flagging them. The
  `_ScratchAstGate = _MPFR` pattern the subagent initially produced this
  session was caught and simplified by the orchestrator. The simpler
  bare type-only import is the canonical workaround until `mpfr-ts-l4t`
  resolves.
- Pad goldens with synthetic cases to meet Rule 7 minimums on no-arg ports.
  The carve-out is the policy; the per-driver header comment documents it.
- Run `ralph.py --parallel N` with N > 10.
- Ship a port without mutation-prove. The vacuous-pass carve-out (worklog
  011) handles trivial bodies legitimately; `survived` status flags
  golden insufficiency and warrants human review.

## Pickup-on-different-device checklist

1. `git clone git@github.com:tobiasosborne/cs/mpfr-ts.git`
2. `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. `curl -fsSL https://bun.sh/install | bash`
4. `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. `pip install pytest`
6. `bd bootstrap --yes && bd hooks install && bd import`
7. Smoke-check:
   - `cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q` # 119 pass
   - `bash eval/golden_master/build.sh` # all drivers compile
   - All 4 newly-shipped accessor ports grade composite=1.0
   - `python3 eval/driver/mutate.py --function mpfr_swap --port src/ops/swap.ts --golden eval/functions/mpfr_swap/golden.jsonl` # vacuous pass

## Open bd issues at session end (16 total)

P3:
- `mpfr-ts-3a9` (new) — Port mpfr_add_z: mpz/bigint integration ADR
- `mpfr-ts-4x5` (new) — Port mpfr_strtofr: string-IO API ADR
- `mpfr-ts-e2n` (new) — Port mpfr_asprintf: format API ADR
- `mpfr-ts-9di` — mutate.py option (b)/(c) for applied-but-survived
- `mpfr-ts-i8e` — git pre-commit hook for bd auto-export
- `mpfr-ts-18x`, `mpfr-ts-2ls`, `mpfr-ts-ai4`, `mpfr-ts-d6o`,
  `mpfr-ts-e4j`, `mpfr-ts-sr4`

P4:
- `mpfr-ts-l4t` (new) — AST gate require-core-import friction
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

## One final thing

This session's pattern — heavy triage + light execution — is likely the
shape of Production-phase work as the picker climbs into the static-helper
band. The 4 ports shipped per $0.31 in subagent cost; the 9 parks/blocks
cost ~0 (inline state.db updates). Compare to a naive "dispatch one subagent
per candidate" approach: ~$3-5 for the same outcome, with subagents writing
detailed parking specs that the orchestrator could have written in a few
seconds.

The next session's substrate batch (Priority 1) is the opposite shape:
fewer functions, more LOC per port, higher leverage per port. Both
patterns coexist under the Production policy; the orchestrator's job
is to pick the right one per batch.

Good luck.
