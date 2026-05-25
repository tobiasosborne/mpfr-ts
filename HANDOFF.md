# Handoff — 148 ports, mega-batch shipped, carve-out validated

You are picking up mpfr-ts after a 10-port mega batch (worklog 017)
that shipped `mpn_divrem`, `mpn_divrem_1`, `mpfr_nextabove`,
`mpfr_nextbelow`, `mpfr_nbits_ulong`, `mpfr_scale2`, and 4×
`mpfr_buildopt_*_p` — all composite=1.0. The batch emptied the
pending queue and validated the worklog 016 `'low-confidence-pass'`
carve-out in production. State.db: **148 done, 17 blocked, 0 pending.**

Three patterns worth knowing:

1. **Carve-out works.** `mpfr_scale2` was the first production port
   to land in `'low-confidence-pass'`. The predicate's strictness
   (count <= 2 AND composite > 0.99) holds — none of the 10 ports
   got stuck in `'survived'`, which is exactly what `mpfr-ts-9di`
   was filed to eliminate. The 5 other applied-but-survived legacy
   cases re-graded by worklog 016 also remain stably in the bucket.

2. **Serial subagent dispatch survives a 7-dispatch batch with zero
   529s.** This contrasts with worklog 015 (2 of 5 dispatches
   overloaded). Recommend serial as the new default for batches of
   ~10 ports.

3. **Calibration-first discipline caught a real reference-port bug**
   (`scale2`'s `MPFR_ASSERTD` over-validation). Without it, the bug
   would have replicated and shipped at composite=0.9935. See
   gotcha #4 below.

Cumulative cost across all batches: ~$4.60.

## ⚠ Four gotchas — read first

1. **mpz API ADR is now blocking 3 functions.** `mpfr_add_z`,
   `mpfr_set_z_2exp`, `mpfr_get_z_2exp` all blocked on bd
   `mpfr-ts-3a9`. As the rank-list moves up, more `_z` variants
   will accumulate. Worth addressing the ADR soon. **This is now
   Priority 1 (was P4 last session).**

2. **`bd` doesn't auto-export to JSONL on manual commits.** Run
   `bd export -o .beads/issues.jsonl` before `git commit`. Tracked
   by `mpfr-ts-i8e`.

3. **Hex literal hygiene** — driver PRNG seed constants must be
   actual hex (0-9, A-F). Don't use mnemonic letters. (Caught 3x
   in worklog 014; clean in 015, 016, 017.)

4. **NEW: `MPFR_ASSERTD` is debug-only — do NOT over-validate in
   the TS port.** Release builds (the `golden_driver` target) treat
   `MPFR_ASSERTD` as a no-op. If a TS port throws on a condition
   the C source only debug-asserts, the port will fail edge cases
   that fall outside the debug bound but inside the real domain.
   (Surfaced in `scale2` worklog 017; the C source had
   `MPFR_ASSERTD(-1073 <= exp && exp <= 1025)` but the function
   handles `exp=-1074` cleanly via the subnormal branch.)

## TL;DR — first 10 minutes

```bash
git pull --rebase
cat PHASE.md                                          # → Production
cat HANDOFF.md                                        # this file
cat docs/worklog/017-mega-batch-10.md                 # latest session
cat docs/worklog/016-mutate-carve-out.md              # carve-out shipped
cat docs/worklog/015-rint-cluster-batch.md            # rank-15 cluster
cat docs/adr/0002-approximation-helper-grading.md     # parking rules

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|17 done|148 pending|0

cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q   # 123 pass
bash eval/golden_master/build.sh                      # all drivers compile

# Smoke-check 3 representative ports from this session's 10:
for fn in mpn_divrem mpfr_scale2 mpfr_nbits_ulong; do
  case $fn in
    mpn_*) port=src/internal/mpn/${fn#mpn_}.ts ;;
    mpfr_*) port=src/ops/${fn#mpfr_}.ts ;;
  esac
  bun eval/harness/runner.ts --function $fn --port $port \
    --golden eval/functions/$fn/golden.jsonl --output /tmp/v.json
done

bd ready                                              # 16 issues
```

## Next-session priority sequence

### Priority 1 (START HERE): Resolve mpz API ADR (`mpfr-ts-3a9`)

**Why now**: blocking 3 functions (`mpfr_add_z`, `mpfr_set_z_2exp`,
`mpfr_get_z_2exp`), and will accumulate as the picker climbs ranks —
every `mpfr_*_z` variant in the call graph waits on this. Worklog
015 blocked 2 of these and worklog 017 didn't pick up any `_z`
functions, but the next 10-15-port batch likely will. Filing a
focused ADR session now unblocks compound downstream value.

**Deliverable**: 1 ADR (`docs/adr/0003-mpz-api.md`) + 1-3 reference
ports (`mpfr_add_z`, `mpfr_set_z_2exp`, `mpfr_get_z_2exp`)
demonstrating the chosen pattern.

**Chosen-pattern candidates**: (a) expose `_z` variants taking
native TypeScript `bigint`, or (b) recommend users compose via
`mpfr_set_from_bigint + arithmetic`. Option (a) preserves API parity
with libmpfr; option (b) keeps the surface smaller. Decide based on
how MPFR users actually compose downstream operations — check the
test corpus for `_z` chain usage.

Estimated effort: 1-3 hours.

### Priority 2: Enqueue + ship next 10-15 ports

The pending queue is empty. Run `ralph.py --next --batch-size 10` to
surface the next tier (rank 217+). Per worklog 017, per-port time
was 1.5-12 min and cost was ~$0.10-0.30 — strong throughput.

The serial-dispatch discipline from worklog 017 (one subagent at a
time, zero 529s across 7 dispatches) is the new default. Batch the
PREP work for all 10 functions into a single subagent; then dispatch
PORTs one-at-a-time matched by surface shape (substrate / middle-tier
/ trivial primitives — see worklog 017 batch-composition section).

Estimated cost: $3-5 per batch of 10 ports.

### Priority 3: Resolve `mpfr-ts-ndc` (state.db port_path tmpdirs)

Low-risk band-aid: `state.db` sometimes records `port_path` as a
vanished `/tmp/eval_<fn>/port.ts`. Prefer to address as part of a
small `--ship` / `--grade` resolver tidy (worklog 017 friction #3):
make both paths share `resolve_port_path` so `--ship` no longer
requires `/tmp` staging. Low priority — the workaround (substitute
canonical `src/...` path) is reliable.

Estimated effort: ~30 LOC harness patch + tests. 1 hour.

### Priority 4-N: Other open P3/P4 issues (carried)

- `mpfr-ts-4x5`, `mpfr-ts-e2n` — string-IO and printf API ADRs
- `mpfr-ts-i8e` — git pre-commit hook for bd export
- `mpfr-ts-l4t` — AST gate require-core-import friction
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
| AST gate | `eval/harness/ast_check.ts` | Solid; friction on no-arg ports tracked by mpfr-ts-l4t |
| Substrate (mpn) | `src/internal/mpn/` | 12 files (+`divrem.ts`, `divrem_1.ts`) |
| Substrate (mpfr) | `src/internal/mpfr/` | 12 files |
| Callgraph | `eval/driver/callgraph.json` | 525 fns |
| State DB | `eval/state.db` | 165 rows; 148 done, 17 blocked, 0 pending |
| ralph picker | `eval/driver/ralph.py --next` | seed + select |
| mutate.py | `eval/driver/mutate.py` | gate_status: killed/vacuous/survived/**low-confidence-pass**; carve-out validated in worklog 017 |
| ADR 0001, 0002 | `docs/adr/` | Both load-bearing |
| rank-15 cluster ports | `src/ops/{nexttozero,nexttoinf,rint,sub1sp}.ts` | All composite=1.0 |
| **NEW**: mega-batch 10 ports | `src/ops/{nextabove,nextbelow,nbits_ulong,scale2,buildopt_*_p}.ts` + `src/internal/mpn/{divrem,divrem_1}.ts` | All composite=1.0 |

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR.
- Modify ADR 0001 or 0002 without writing a successor.
- Skip `bd export -o .beads/issues.jsonl` before `git commit`.
- Add dead code to port files to satisfy mutate.py (the carve-out
  handles the legitimate thin-surface cases; gaming the gate
  destroys signal value).
- Use mnemonic letters in C hex literals (only 0-9, A-F).
- Run `ralph.py --parallel N` with N > 10.
- Manually port before generating the golden — let libmpfr tell you
  the actual contract first.
- **NEW**: Add `MPFR_ASSERTD`-as-throw validation in TS ports. The
  C assertion is debug-only; release ignores it. (worklog 017
  `scale2` bug.)

## Pickup-on-different-device checklist

1. `git clone git@github.com:tobiasosborne/mpfr-ts.git`
2. `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. `curl -fsSL https://bun.sh/install | bash`
4. `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. `pip install pytest`
6. `bd bootstrap --yes && bd hooks install && bd import`
7. Smoke-check:
   - `cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q` # 123 pass
   - `bash eval/golden_master/build.sh` # all drivers compile
   - 3 representative mega-batch ports grade composite=1.0 (per TL;DR loop).
8. Read CLAUDE.md → this file → worklog 017 → 016 → 015 → 014 → ADR 0001 / 0002.

## Open bd issues at session end (16 total — unchanged)

`mpfr-ts-9di` was closed in commit 8f34c7a (worklog 016 carve-out
shipped). `mpfr-ts-ndc` filed last session (state.db port_path
tmpdirs) remains open at P3.

P3:
- `mpfr-ts-3a9` — **NOW PRIORITY 1**: mpz API ADR (3 blocked, more
  accumulating)
- `mpfr-ts-4x5`, `mpfr-ts-e2n` — API-decision ADRs
- `mpfr-ts-i8e` — git pre-commit hook
- `mpfr-ts-ndc` — state.db port_path tmpdirs
- `mpfr-ts-18x`, `mpfr-ts-2ls`, `mpfr-ts-ai4`, `mpfr-ts-d6o`,
  `mpfr-ts-e4j`, `mpfr-ts-sr4`

P4:
- `mpfr-ts-l4t` — AST gate require-core-import friction
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

## One final thing

This is the first true mega batch since worklog 007's 30-fn-mega.
Three things made it possible: (a) the worklog 014 substrate unlock
(101 newly-eligible downstream functions, only ~14 of which had been
harvested before this batch), (b) the worklog 016 carve-out shipping
cleanly to production (validated by `mpfr_scale2` landing in
`'low-confidence-pass'`), and (c) the serial-dispatch discipline
holding for 7 consecutive subagent calls with zero 529s.

The picker queue + subagent dispatch flow is now reliable for
batches of this size. Recommend continuing the cadence: resolve the
mpz ADR (P1), then run another 10-15-port batch (P2). The cost curve
is favorable (~$2-5 per batch) and the gate-noise is no longer
masking real signal.

Good luck.
