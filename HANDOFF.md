# Handoff — 174 ports, second mega batch shipped, value_codec is now Priority 1

You are picking up mpfr-ts after a 26-port mega batch (worklog 018).
28 functions prepped, 26 shipped at composite=1.0, 2 carved out to
`mpfr-ts-2ls` (scalar-string output codec gap). State.db:
**174 done · 21 blocked · 0 pending.** Cumulative cost across all
batches: ~$7.50.

Three patterns worth knowing:

1. **Calibration-first discipline keeps paying off.** Worklog 017
   caught a `scale2` MPFR_ASSERTD bug. Worklog 018 caught 5 issues —
   one real C-compat trap (`mpfr_eq` n_bits=0 unsigned-underflow) and
   4 weak reference ports. All fixed before PORT subagents ran. Without
   this discipline, a real bug would have shipped at composite=0.99.

2. **"Collapse decision tree to constant" beats "perturb one branch"
   for broken reference ports.** 3 of 4 broken-fix-ups switched from
   narrow perturbations (one swap, one off-by-one) to constant returns.
   The narrow form left too many cases passing because goldens
   typically exercise the OTHER branches uniformly. Worth a PREP-prompt
   update for the next batch.

3. **Serial-dispatch discipline holds at 4 dispatches with zero 529s.**
   PREP + 3 PORTs over ~95 min wall. Each PORT subagent fast (4-25 min
   wall, $0.30-0.50 cost). PREP remains the dominant cost (~$2 for 28
   functions; this is the "amortize across batches" sweet spot).

## ⚠ Gotchas — read first

1. **`mpfr-ts-2ls` (value_codec scalar-string outputs) is now blocking
   real production work.** Worklog 018 carved out `mpfr_fdump` and
   `mpfr_buildopt_tune_case`. As the picker climbs ranks, more
   string-output functions will accumulate (`mpfr_get_str` eventually).
   **This is now Priority 1 (was P3 last session).** Two-part fix:
   add `string` branch to value_codec's parseGoldenOutput +
   compareOutput, AND fix `mpfr_fdump`'s golden_driver to JSON-escape
   its output. ~$0.30 / 1-2 hours estimated.

2. **`mpfr-ts-3a9` (mpz API ADR) still blocks 5 functions** (`mpfr_add_z`,
   `mpfr_set_z_2exp`, `mpfr_get_z_2exp`, `mpfr_mul_z`, `mpfr_sub_z`).
   Demoted from Priority 1 only because `mpfr-ts-2ls` is now larger
   blast radius. **Still Priority 2.**

3. **`bd` doesn't auto-export to JSONL on manual commits.** Run
   `bd export -o .beads/issues.jsonl` before `git commit`. Tracked
   by `mpfr-ts-i8e`.

4. **Hex literal hygiene** — driver PRNG seed constants must be
   actual hex (0-9, A-F). Don't use mnemonic letters.

5. **`MPFR_ASSERTD` is debug-only — do NOT over-validate in
   the TS port.** Release builds (the `golden_driver` target) treat
   `MPFR_ASSERTD` as a no-op. If a TS port throws on a condition
   the C source only debug-asserts, the port will fail edge cases
   that fall outside the debug bound but inside the real domain.

6. **NEW (worklog 018): C unsigned-arithmetic traps in comparison
   functions.** `mpfr_eq(u, v, n_bits=0)` triggers an unsigned
   underflow in `1 + (n_bits - 1) / GMP_NUMB_BITS` that causes
   libmpfr to compare the top 64 bits rather than "compare nothing."
   When porting comparison/equality functions, audit C unsigned
   arithmetic for `n - 1` patterns; mirror the effective behavior,
   not the apparent semantics.

## TL;DR — first 10 minutes

```bash
git pull --rebase
cat PHASE.md                                          # → Production
cat HANDOFF.md                                        # this file
cat docs/worklog/018-mega-batch-26.md                 # latest session
cat docs/worklog/017-mega-batch-10.md                 # prior session
cat docs/worklog/016-mutate-carve-out.md              # carve-out shipped
cat docs/adr/0002-approximation-helper-grading.md     # parking rules

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|21 done|174 pending|0

cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q   # 123 pass
bash eval/golden_master/build.sh                      # all drivers compile

# Smoke-check 3 representative ports from worklog 018:
for fn in mpfr_eq mpfr_sum mpfr_compound_near_one; do
  bun eval/harness/runner.ts --function $fn --port src/ops/${fn#mpfr_}.ts \
    --golden eval/functions/$fn/golden.jsonl --output /tmp/v.json
done

bd ready                                              # 16 issues
```

## Next-session priority sequence

### Priority 1 (START HERE): Resolve `mpfr-ts-2ls` (value_codec strings)

**Why now**: blocking 2 production-ready ports today (`mpfr_fdump`,
`mpfr_buildopt_tune_case`), and will accumulate as the picker climbs
ranks. The next ~30 functions almost certainly include more
string-output cases. A focused 1-2 hour session would unblock both
today and future work.

**Deliverable**: value_codec updated to handle `'string'` scalar
outputs + fdump's golden_driver fixed to JSON-escape its output.
Re-grade `mpfr_fdump` and `mpfr_buildopt_tune_case` and move them
from blocked → done.

**Implementation sketch**:
  1. `eval/harness/value_codec.ts`:
     - In `parseGoldenOutput` (around L298): add an early return for
       string outputs that match neither numeric tokens nor decimal
       integer strings — return `{ kind: 'scalar', value: raw }`.
     - In `compareOutput` `case 'scalar'`: string equality already
       works once the value comes through as a string.
  2. `eval/functions/mpfr_fdump/golden_driver.c`: replace the raw
     `fprintf(out, "...", str)` with a JSON-escape pass (or use
     `jl_output_scalar_string` if such a helper exists/can be added
     to `common.h`).
  3. Re-run `bash eval/golden_master/run_all.sh --force --filter
     mpfr_fdump` and `--filter mpfr_buildopt_tune_case`.
  4. `python3 eval/driver/ralph.py --grade mpfr_fdump
     mpfr_buildopt_tune_case`.

Estimated effort: 1-2 hours, ~$0.30 cost.

### Priority 2: Resolve `mpfr-ts-3a9` (mpz API ADR)

**Why now**: still blocks 5 functions (`mpfr_add_z`, `mpfr_set_z_2exp`,
`mpfr_get_z_2exp`, `mpfr_mul_z`, `mpfr_sub_z`). Demoted only because
P1 has wider blast radius. Worklog 018 didn't pick up any new `_z`
functions because the picker enqueued only middle-rank candidates;
the next 10-15-port batch likely will.

**Deliverable**: 1 ADR (`docs/adr/0003-mpz-api.md`) + 1-3 reference
ports demonstrating the chosen pattern.

Estimated effort: 1-3 hours.

### Priority 3: Enqueue + ship next 10-30 ports

The pending queue is empty. Run `python3 eval/driver/ralph.py --next
--batch-size 30` to surface the next tier. Per worklog 018, the cost
curve for a 28→26-shipped batch was ~$3 total, well under the $50
ceiling. Recommend continuing the mega-batch cadence; PREP-for-30 in
one shot is the economic sweet spot.

The serial-dispatch discipline from worklogs 017+018 (one subagent at
a time, zero 529s) remains the default. Shape-match the PORT
dispatches (trivial cluster / middle / substantial).

Estimated cost: $3-5 per batch of ~25-30 ports.

### Priority 4: Resolve `mpfr-ts-ndc` (state.db port_path tmpdirs)

Low-risk band-aid; carried from worklog 017. Resolve as part of a
small `--ship` / `--grade` resolver tidy. Workaround (substitute
canonical `src/...` path) remains reliable.

### Priority 5-N: Other open P3/P4 issues (carried)

- `mpfr-ts-4x5`, `mpfr-ts-e2n` — string-IO and printf API ADRs (related
  to but distinct from `mpfr-ts-2ls`)
- `mpfr-ts-i8e` — git pre-commit hook for bd export
- `mpfr-ts-l4t` — AST gate require-core-import friction
- `mpfr-ts-18x`, `mpfr-ts-2ls` (this session: P1), `mpfr-ts-ai4`,
  `mpfr-ts-d6o`, `mpfr-ts-e4j`, `mpfr-ts-sr4` — harness polish
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg` — cleanup

None block scale-out.

## What's working now (don't change)

| Component | Path | Notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Strict equality; substrate exempt from requireCoreImport |
| AST gate | `eval/harness/ast_check.ts` | Solid |
| Substrate (mpn) | `src/internal/mpn/` | 12 files |
| Substrate (mpfr) | `src/internal/mpfr/` | 12 files (`flags.ts` heavily delegated to by worklog 018 clear_* family) |
| Callgraph | `eval/driver/callgraph.json` | 525 fns |
| State DB | `eval/state.db` | 195 rows; 174 done, 21 blocked, 0 pending |
| ralph picker | `eval/driver/ralph.py --next` | seed + select |
| mutate.py | `eval/driver/mutate.py` | gate_status: killed/vacuous/survived/**low-confidence-pass**; carve-out validated in worklogs 017+018 |
| ADR 0001, 0002 | `docs/adr/` | Both load-bearing |
| **NEW**: 26 worklog-018 ports | `src/ops/{buildopt_tls_p,clear_*,custom_get_*,eq,check,custom_init*,custom_move,random_deviate_*,compound_near_one,const_euler_bs_*,const_log2_internal,sum}.ts` | All composite=1.0 |

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR.
- Modify ADR 0001 or 0002 without writing a successor.
- Skip `bd export -o .beads/issues.jsonl` before `git commit`.
- Add dead code to port files to satisfy mutate.py (the carve-out
  handles legitimate thin-surface cases).
- Use mnemonic letters in C hex literals (only 0-9, A-F).
- Run `ralph.py --parallel N` with N > 10.
- Manually port before generating the golden — let libmpfr tell you
  the actual contract first.
- Add `MPFR_ASSERTD`-as-throw validation in TS ports.
- **NEW (018)**: Write narrow-perturbation broken reference ports
  ("swap two branches", "off-by-one constant"). Prefer "collapse the
  entire decision tree to a constant output" — narrow perturbations
  leave too many cases passing and fail calibration.
- **NEW (018)**: Replicate the "delegating fallback" pattern for
  `mpfr_sum` / `mpfr_const_log2_internal` without writing the
  `TODO(optimize-phase)` marker. The pattern is acceptable for hitting
  composite=1.0 against the golden, but the next reader must know
  the port has a restricted-domain caveat.

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
   - 3 representative worklog-018 ports grade composite=1.0 (per TL;DR loop).
8. Read CLAUDE.md → this file → worklog 018 → 017 → 016 → 015 → 014 → ADR 0001 / 0002.

## Open bd issues at session end (16 total)

P1:
- `mpfr-ts-2ls` — **NOW PRIORITY 1**: value_codec scalar-string outputs
  (2 blocked from worklog 018, more accumulating). Promoted from P3.

P2:
- `mpfr-ts-3a9` — mpz API ADR (5 blocked: add_z, set_z_2exp,
  get_z_2exp, mul_z, sub_z). Demoted from P1.

P3:
- `mpfr-ts-4x5`, `mpfr-ts-e2n` — API-decision ADRs (related to 2ls)
- `mpfr-ts-i8e` — git pre-commit hook
- `mpfr-ts-ndc` — state.db port_path tmpdirs
- `mpfr-ts-18x`, `mpfr-ts-ai4`, `mpfr-ts-d6o`, `mpfr-ts-e4j`,
  `mpfr-ts-sr4`

P4:
- `mpfr-ts-l4t` — AST gate require-core-import friction
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

## One final thing

Two mega batches back-to-back (worklog 017 + 018) shipped 36 ports for
~$5. The PREP-PORT economic model is stable — PREP at ~$2 per
~25-function batch, PORTs at ~$0.30-0.50 each. The serial-dispatch
discipline holds (zero 529s across 11 consecutive subagent calls).
Calibration-first catches bugs that would otherwise ship as
`low-confidence-pass` or `survived` noise.

The library is now **174/525 functions = 33% complete**. At ~25 ports
per session and the current cost curve, scale-out for the remaining
~330 unblocked-eligible functions is on the order of $40 across 13-14
sessions — comfortably under any reasonable budget.

The two priorities for next session are:
1. **value_codec strings** (`mpfr-ts-2ls`, P1) — unblocks today's 2
   carve-outs plus future string-IO functions.
2. **Another 25-30 port mega batch** (P3) — keeps the throughput
   momentum.

Good luck.
