# Handoff — 134 ports, 6 substrate primitives shipped, 101 downstream unblocked

You are picking up mpfr-ts after a substrate batch that shipped 6
mpn_* primitives (copyi, copyd, zero, add_1, sub_1, rshift) and
unblocked **101 newly-eligible downstream MPFR functions**. State.db:
**134 done, 14 blocked, 2 pending.**

Two notable session-specific things to know:

1. **Anthropic API was persistently overloaded** for `general-purpose`
   Agent dispatches today (4 consecutive 529s before the orchestrator
   switched to inline execution). The pattern shipped fine inline; the
   inline-fallback discipline is documented in worklog 014. Try
   dispatching next session — by then the API should have recovered.

2. **3 of 6 ports caught real bugs via RED → GREEN** — `mpn_add_1`
   and `mpn_sub_1` had asymmetric n=0 contracts in GMP that intuition
   got wrong, plus the drivers had invalid hex constants. Worklog
   014's "Live bugs caught" section documents each. The harness
   discipline is paying for itself.

Cumulative cost across batches 1+2+3: ~$1.50.

## ⚠ Three gotchas — read first

1. **101 newly-eligible downstream functions ready for the picker.**
   `ralph.py --next --batch-size N` will surface them. Top candidates
   by rank: `mpfr_sub1sp` (15), `mpfr_set_z_2exp` (16), `mpfr_rint`
   (26), `mpfr_addrsh` (36), `mpn_divrem_1` (76), `mpfr_nexttozero`
   (84), etc. The substrate batch this session was specifically to
   unblock these.

2. **Hex literal hygiene for C drivers.** Don't use
   `0xADC0PYDEED`-style mnemonic constants — `P, Y, S, H, U, B` (etc.)
   beyond `0-9, A-F` are illegal in hex. I shipped 3 drivers with this
   bug; all caught by `gcc --error`. When you need a memorable seed,
   stick to actual hex digits or use `_` separators (`0xADD_1_BEEF`).

3. **`bd` doesn't auto-export to JSONL on manual commits.** Run
   `bd export -o .beads/issues.jsonl` before `git commit` or use
   `ralph.py --commit-batch`/`--ship` flow. Tracked by `mpfr-ts-i8e`.

## TL;DR — first 10 minutes

```bash
git pull --rebase
cat PHASE.md                                          # → Production
cat HANDOFF.md                                        # this file
cat docs/worklog/014-substrate-batch.md               # latest session
cat docs/worklog/013-second-production-batch.md       # previous
cat docs/adr/0002-approximation-helper-grading.md     # load-bearing

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|14 done|134 pending|2

cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q   # 119 pass
bash eval/golden_master/build.sh                      # all drivers compile

# Smoke-check the 6 substrate primitives shipped this session:
for fn in mpn_copyi mpn_copyd mpn_zero mpn_add_1 mpn_sub_1 mpn_rshift; do
  base=${fn#mpn_}
  bun eval/harness/runner.ts --function $fn --port src/internal/mpn/$base.ts \
    --golden eval/functions/$fn/golden.jsonl --output /tmp/v.json
done
# Expected: all 6 composite=1.0

# Survey newly-eligible work:
python3 eval/driver/ralph.py --list-pending | head -20
# OR survey what NEW would surface:
python3 eval/driver/ralph.py --next --batch-size 6 2>&1 | grep '^SELECTED'

bd ready                                              # 16 issues (unchanged)
```

## Next-session priority sequence

### Priority 1 (recommended): Pick up the rank-15 cluster

The substrate batch was specifically to unblock these — pick them up
to harvest the leverage:

| Rank | Function | What it does |
|---:|---|---|
| 15 | `mpfr_sub1sp` | same-prec subtraction wrapper |
| 16 | `mpfr_set_z_2exp` | set MPFR from bigint × 2^exp |
| 26 | `mpfr_rint` | round-to-int dispatcher |
| 36 | `mpfr_addrsh` | add-right-shift internal helper |
| 84 | `mpfr_nexttozero` | next FP value toward zero |
| 85 | `mpfr_nexttoinf` | next FP value toward infinity |

These are all rank-15-to-85 — earliest in topo order, highest value.
Expect 5-8 ports per batch. If API dispatch is working, ~$3-5 in
subagent costs.

`mpfr_sub1sp` is large (full subtraction-same-precision algorithm,
~300 LOC of MPFR source); `mpfr_addrsh` and `mpfr_rint` are
medium-sized; `mpfr_nexttozero`/`nexttoinf` are small.

**Deliverable**: 5-8 ports + worklog 015 + HANDOFF refresh.

### Priority 2: `mpn_divrem_1` (rank 76, substrate)

The next substrate primitive in the callgraph. ~100 LOC of multi-
precision division, depends on `mpn_copyi` and `mpn_zero` (both
shipped this session). Unlocks `mpn_divrem` and `mpn_tdiv_qr`
(higher-rank substrate).

Estimated cost: ~$1-2 in subagent dispatches (one fairly substantial
port). Effort: 1-2 hours.

### Priority 3: `mpfr-ts-l4t` cleanup (P4)

Strip the dead `import type { MPFR as _MPFR } from '../core.ts'`
imports from the 4 no-arg accessor ports (buildopt_bfloat16_p,
buildopt_decimal_p, get_emin, get_emax) by adding either an
auto-detect carve-out or a `pure-utility` portClass to the runner.

Estimated effort: 30 minutes.

### Priority 4-N: Other open P3/P4 issues (carried)

- `mpfr-ts-3a9`, `mpfr-ts-4x5`, `mpfr-ts-e2n` — API-decision ADRs
- `mpfr-ts-9di` — mutate.py option (b)/(c) for applied-but-survived
- `mpfr-ts-i8e` — git pre-commit hook for bd export
- `mpfr-ts-18x`, `mpfr-ts-2ls`, `mpfr-ts-ai4`, `mpfr-ts-d6o`,
  `mpfr-ts-e4j`, `mpfr-ts-sr4` — harness polish
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg` — cleanup

None block scale-out.

## What's working now (don't change)

| Component | Path | Notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Strict equality; substrate exempt from requireCoreImport |
| AST gate | `eval/harness/ast_check.ts` | Friction on no-arg ports — bd `mpfr-ts-l4t` |
| Substrate (mpn) | `src/internal/mpn/` | **10 files now**: add_n, cmp, lshift, sub_n, copyi, copyd, zero, add_1, sub_1, rshift |
| Substrate (mpfr) | `src/internal/mpfr/` | 12 files |
| Callgraph | `eval/driver/callgraph.json` | 525 fns; some primitives external (not seeded automatically) |
| State DB | `eval/state.db` | 150 rows; 134 done, 14 blocked, 2 pending |
| gen_spec | `eval/driver/gen_spec.py` | arg order `(c_source_path, function_name)` |
| mutate.py | `eval/driver/mutate.py` | gate_status: killed/vacuous/survived |
| ralph picker | `eval/driver/ralph.py --next` | seeds + selects |
| ADR 0001, 0002 | `docs/adr/` | Both load-bearing |

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR.
- Modify ADR 0001 or 0002 without writing a successor.
- Skip `bd export -o .beads/issues.jsonl` before `git commit` (or
  use the `--commit-batch`/`--ship` ralph flow instead).
- Add dead code to port files to satisfy mutate.py (worklog 010
  lesson; live-caught again this session via the `_ScratchAstGate`
  pattern in batch 2).
- Skip golden-driver hex digit validation. `0x[^0-9A-F]` will not
  compile.
- Manually port a function before generating its golden. Lesson from
  this session: GMP n=0 contracts are surprising; let libgmp tell you
  the actual behavior first, then write the port to match.
- Run `ralph.py --parallel N` with N > 10.

## Pickup-on-different-device checklist

1. `git clone git@github.com:tobiasosborne/cs/mpfr-ts.git`
2. `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. `curl -fsSL https://bun.sh/install | bash`
4. `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. `pip install pytest`
6. `bd bootstrap --yes && bd hooks install && bd import`
7. Smoke-check:
   - `cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q` # 119
   - `bash eval/golden_master/build.sh` # all drivers compile
   - Verify the 6 new substrate primitives grade composite=1.0 (per
     loop in TL;DR above).
8. Read CLAUDE.md → this file → `docs/worklog/014-substrate-batch.md`
   → 013 → 012 → 011 → ADR 0001 / 0002.

## Open bd issues at session end (16 total)

No new issues this session.

P3:
- `mpfr-ts-3a9`, `mpfr-ts-4x5`, `mpfr-ts-e2n` — API-decision ADRs
- `mpfr-ts-9di` — mutate.py applied-but-survived (now 6 live examples)
- `mpfr-ts-i8e` — git pre-commit hook
- `mpfr-ts-18x`, `mpfr-ts-2ls`, `mpfr-ts-ai4`, `mpfr-ts-d6o`,
  `mpfr-ts-e4j`, `mpfr-ts-sr4` — harness polish

P4:
- `mpfr-ts-l4t` — AST gate require-core-import friction
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

## One final thing

This session was an unintentional stress-test of the orchestrator
inline-fallback pattern. Four consecutive API 529s on subagent
dispatches would have stalled a less flexible workflow; instead the
orchestrator absorbed the work and shipped 6 substrate ports.
Importantly, the inline pattern doesn't *replace* subagent dispatch —
it complements it for work shapes that are well-structured and
repetitive (substrate primitives, accessor ports). For larger
algorithmic ports (the rank-15 cluster recommended for next session),
subagent dispatch remains the right choice when the API is healthy.

The substrate batch's 25× leverage delta (6 ports → 101 newly-eligible
downstream) confirms the HANDOFF 012/013 sequencing advice: invest in
substrate-depth before surface-breadth when downstream is constrained.
The next session's rank-15 cluster pickup is the harvest of this
session's investment.

Good luck.
