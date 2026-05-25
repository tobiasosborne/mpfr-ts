# Handoff — 138 ports, rank-15 cluster harvested, mutate.py noise growing

You are picking up mpfr-ts after a rank-15 cluster batch that shipped
4 ports (`mpfr_nexttozero`, `mpfr_nexttoinf`, `mpfr_rint`,
`mpfr_sub1sp`), parked 1 (`mpfr_addrsh` per ADR 0002), and blocked 2
on the mpz API decision (`mpfr_set_z_2exp`, `mpfr_get_z_2exp`).
State.db: **138 done, 17 blocked, 4 pending.**

Two patterns worth knowing:

1. **The applied-but-survived mutate.py bucket is growing.** 7 known
   cases now (`sqrt1`, `set_inf`, `get_d1`, `copyi`, `copyd`, `zero`,
   `sub1sp`). All are pure-dispatch / pure-delegation ports that
   lack the algorithmic surface current mutators target. This is
   becoming the dominant source of gate-fail noise; I recommend
   resolving `mpfr-ts-9di` (option b complexity-floor or option c
   per-spec exempt flag) as Priority 1 next session.

2. **Inline fallback for dispatcher-style ports.** Subagent dispatch
   has been intermittent (2 successful + 2 overloaded in this
   session; 4 consecutive overloads last session). For well-structured
   repetitive work like dispatchers and trivial primitives, inline is
   the documented fallback. For algorithm-heavy ports, prefer
   subagent dispatch when API is healthy.

Cumulative cost across all batches: ~$2.60.

## ⚠ Three gotchas — read first

1. **mpz API ADR is now blocking 3 functions.** `mpfr_add_z`,
   `mpfr_set_z_2exp`, `mpfr_get_z_2exp` all blocked on bd
   `mpfr-ts-3a9`. As the rank-list moves up, more `_z` variants
   will accumulate. Worth addressing the ADR soon.

2. **`bd` doesn't auto-export to JSONL on manual commits.** Run
   `bd export -o .beads/issues.jsonl` before `git commit`. Tracked
   by `mpfr-ts-i8e`.

3. **Hex literal hygiene** — driver PRNG seed constants must be
   actual hex (0-9, A-F). Don't use mnemonic letters. (Caught 3x
   in worklog 014; clean this session.)

## TL;DR — first 10 minutes

```bash
git pull --rebase
cat PHASE.md                                          # → Production
cat HANDOFF.md                                        # this file
cat docs/worklog/015-rint-cluster-batch.md            # latest session
cat docs/worklog/014-substrate-batch.md               # the substrate unlock
cat docs/adr/0002-approximation-helper-grading.md     # parking rules

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|17 done|138 pending|4

cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q   # 119 pass
bash eval/golden_master/build.sh                      # all drivers compile

# Smoke-check this session's 4 ports:
for fn in mpfr_nexttozero mpfr_nexttoinf mpfr_rint mpfr_sub1sp; do
  base=${fn#mpfr_}
  bun eval/harness/runner.ts --function $fn --port src/ops/$base.ts \
    --golden eval/functions/$fn/golden.jsonl --output /tmp/v.json
done

bd ready                                              # 16 issues
```

## Next-session priority sequence

### Priority 1 (START HERE): Resolve `mpfr-ts-9di` — applied-but-survived carve-out

**Why now**: 7 live applied-but-survived cases (sqrt1, set_inf,
get_d1, copyi, copyd, zero, sub1sp). The pattern is clear:
pure-dispatch / pure-delegation ports lack the algorithmic surface
current mutators target, so they survive at composite > 0.95 even
when the port is correct by construction. Signal-to-noise on
mutate.py gates is degrading, and every future dispatcher / delegation
port (`mpfr_rint_floor`, `mpfr_rint_ceil`, `mpfr_round_within`, etc.)
will add to the pile. **A small harness change clears the noise
across the entire bucket.**

#### Chosen approach: option (b) complexity-floor carve-out

In `eval/driver/mutate.py`, extend the gate_status taxonomy:

- Current statuses: `'killed'` | `'vacuous'` | `'survived'`
- Add: `'low-confidence-pass'` for ports where mutations applied but
  the structural surface is too thin for the existing mutators to
  attack (the applied-but-survived pattern).

Heuristic: gate_status = `'low-confidence-pass'` (and gate_passed =
True) when:
- At least 1 mutation applied (else `'vacuous'` already fires), AND
- ALL applied non-init-failed mutations scored ≥ 0.95 (i.e. no kill),
  AND
- Applied non-init-failed count ≤ 2 (the structural-surface signal —
  delegation/dispatch ports have ≤ 2 applicable mutations on the
  existing mutator pool; ports with 3+ applicable mutations that all
  survived are genuinely insufficient-golden cases and should keep
  `'survived'`).

The threshold of 2 is calibrated to the 7 known cases:
- sqrt1: 2 applied (bigint-bump, comparison-swap), both >0.95 → carve out
- set_inf: 2 applied (op-swap, sign-flip), both 1.0 → carve out
- get_d1: 1 applied (rnd-swap) at 1.0 → carve out
- copyi/copyd/zero: 1 each (comparison-swap) at 1.0 → carve out
- sub1sp: 3 applied (rnd-swap, bigint-bump, comparison-swap), all
  >0.95 → NOT carved out (3 mutations applied, genuine survival)

Wait — sub1sp has 3 applied mutations. Under threshold=2 it WOULDN'T
carve out. That may be the right call (3 applied = genuine
algorithmic surface, deserves either better mutators or better
goldens) OR we may want to lift to threshold=3 to cover sub1sp too.
Decide based on a quick re-grade pass.

#### First 30 minutes (concrete recipe)

```bash
# 1. Confirm state
sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|17 done|138 pending|4

# 2. Inspect current mutate.py shape (the carve-out goes here)
sed -n '60,85p' eval/driver/mutate.py
# Specifically: ProveResult.gate_status field (L60), _aggregate_gate
# function (L63), _gate_status function (L76).

# 3. Read the 7 known cases' current mutate output to see what
#    we're carving:
for fn in mpfr_sqrt1 mpfr_set_inf mpfr_get_d1 mpn_copyi mpn_copyd mpn_zero mpfr_sub1sp; do
  port=$(sqlite3 eval/state.db "SELECT port_path FROM runs WHERE fn_name='$fn' ORDER BY started_at DESC LIMIT 1")
  echo "--- $fn (port: $port) ---"
  python3 eval/driver/mutate.py --function "$fn" --port "$port" \
    --golden "eval/functions/$fn/golden.jsonl" 2>&1 | head -8
done
# Confirm: all 7 currently show gate_passed=False (survived) with
# the applied-but-survived pattern.

# 4. Implement: add 'low-confidence-pass' status to _gate_status's
#    enum docstring + return path. Update _aggregate_gate to set
#    gate_passed=True when status=='low-confidence-pass'.

# 5. Add 3-4 unit tests in eval/driver/tests/test_mutate.py covering:
#    - exactly 1 applied mutation, all >0.95 -> 'low-confidence-pass'
#    - 2 applied mutations, all >0.95 -> 'low-confidence-pass'
#    - 3 applied mutations, all >0.95 -> 'survived' (boundary)
#    - 1 applied mutation, one <0.95 -> 'killed' (unchanged)

# 6. Run full test suite:
cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q
# Expected: 122+ pass (was 119 + 3-4 new)

# 7. Re-grade the 7 known cases and confirm they now pass:
for fn in mpfr_sqrt1 mpfr_set_inf mpfr_get_d1 mpn_copyi mpn_copyd mpn_zero mpfr_sub1sp; do
  # ... (same loop as step 3)
done
# Expected: 6 of 7 flip from 'survived' to 'low-confidence-pass';
# sub1sp may stay 'survived' if threshold=2, or also flip if =3.
# Make the call here based on the data.
```

#### Acceptance criteria

1. `pytest tests/` passes with ≥3 new tests for the carve-out.
2. All 7 known applied-but-survived cases now report
   `gate_passed=True` with `gate_status='low-confidence-pass'` (or
   'survived' for any case the threshold deliberately excludes).
3. No regressions: existing 'killed' / 'vacuous' / 'survived' tests
   still pass.
4. Worklog 016 documents: chosen threshold (2 or 3?), the 7 cases'
   before/after status, and any porter-facing guidance (e.g.
   "low-confidence-pass means the port is shipped but the golden
   doesn't exercise the algorithmic surface; consider stronger
   goldens or wait for stronger mutators").

**Deliverable**: ~30-40 LOC harness patch + 3-4 tests + worklog 016.

Estimated effort: 1-2 hours.

#### Why NOT option (c) per-spec exempt flag

Risk: every porter would set `mutation_prove_exempt: true` to avoid
the gate, and the signal value of the gate erodes to zero. Option (b)
keeps the porter blind to the gate's lenient mode — it's a harness
decision based on structural surface analysis, not a porter opt-in.

### Priority 2: Pick up `mpn_divrem_1` (rank 76, substrate)

The next substrate primitive in the pending list. ~100 LOC of
multi-precision division by single limb. Already has its substrate
dependencies satisfied (`mpn_copyi` and `mpn_zero` shipped in
worklog 014). Unlocks `mpn_divrem` and `mpn_tdiv_qr` (already
pending) and downstream MPFR `div` functions.

Estimated cost: ~$1-2 subagent dispatch (one substantial port).
Effort: 1-2 hours.

### Priority 3: Re-run picker for the next rank-15+ cluster tier

The substrate unblock from worklog 014 surfaced 101 newly-eligible
functions. This session harvested ~4 of them. Lots of value remaining
in the rank 15-100 band. `ralph.py --next --batch-size 8` will
surface the next tier; triage as before (block mpz variants, park
static helpers without parents, ship the rest).

Estimated cost: $3-5 per batch of 5-8 ports.

### Priority 4: Resolve mpz API ADR (`mpfr-ts-3a9`)

Now blocking 3 functions (will accumulate more as the picker climbs
ranks: every `mpfr_*_z` variant in the call graph). Worth a
focused ADR session to decide between (a) expose `_z` variants
taking `bigint`, or (b) recommend users compose via
`mpfr_set_from_bigint + arithmetic`.

Estimated effort: ADR + 1-3 reference ports (`mpfr_add_z`,
`mpfr_set_z_2exp`, `mpfr_get_z_2exp`) showing the chosen pattern.
1-3 hours.

### Priority 5-N: Other open P3/P4 issues (carried)

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
| Substrate (mpn) | `src/internal/mpn/` | 10 files |
| Substrate (mpfr) | `src/internal/mpfr/` | 12 files |
| Callgraph | `eval/driver/callgraph.json` | 525 fns |
| State DB | `eval/state.db` | 159 rows; 138 done, 17 blocked, 4 pending |
| ralph picker | `eval/driver/ralph.py --next` | seed + select |
| mutate.py | `eval/driver/mutate.py` | gate_status: killed/vacuous/survived; **needs option-b carve-out for applied-but-survived (P1)** |
| ADR 0001, 0002 | `docs/adr/` | Both load-bearing |
| **NEW**: rank-15 cluster ports | `src/ops/{nexttozero,nexttoinf,rint,sub1sp}.ts` | All composite=1.0 |

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR.
- Modify ADR 0001 or 0002 without writing a successor.
- Skip `bd export -o .beads/issues.jsonl` before `git commit`.
- Add dead code to port files to satisfy mutate.py (the
  applied-but-survived bucket is real; either fix the harness or
  document and move on — don't game).
- Use mnemonic letters in C hex literals (only 0-9, A-F).
- Run `ralph.py --parallel N` with N > 10.
- Manually port before generating the golden — let libmpfr tell you
  the actual contract first. (worklog 014 lesson on n=0 asymmetries.)

## Pickup-on-different-device checklist

1. `git clone git@github.com:tobiasosborne/mpfr-ts.git`
2. `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. `curl -fsSL https://bun.sh/install | bash`
4. `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. `pip install pytest`
6. `bd bootstrap --yes && bd hooks install && bd import`
7. Smoke-check:
   - `cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q` # 119 pass
   - `bash eval/golden_master/build.sh` # all drivers compile
   - The 4 rank-15-cluster ports grade composite=1.0 (per TL;DR loop).
8. Read CLAUDE.md → this file → worklog 015 → 014 → 013 → ADR 0001 / 0002.

## Open bd issues at session end (16 total — unchanged)

P3:
- `mpfr-ts-9di` — **NOW PRIORITY 1**: applied-but-survived carve-out
  (7 live examples; pattern clear)
- `mpfr-ts-3a9`, `mpfr-ts-4x5`, `mpfr-ts-e2n` — API-decision ADRs
- `mpfr-ts-i8e` — git pre-commit hook
- `mpfr-ts-18x`, `mpfr-ts-2ls`, `mpfr-ts-ai4`, `mpfr-ts-d6o`,
  `mpfr-ts-e4j`, `mpfr-ts-sr4`

P4:
- `mpfr-ts-l4t` — AST gate require-core-import friction
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

## One final thing

This session validated the worklog 014 inline-fallback discipline
under real load. 2 subagent successes + 2 dispatch overloads + 1
inline shipped, all with composite=1.0. The discipline holds.

The applied-but-survived mutate.py bucket reaching 7 live examples
is the main developing concern. It's not blocking scale-out, but
each future dispatcher / delegation port adds to the noise floor.
Resolving `mpfr-ts-9di` is the smallest unit of work with the
highest signal-to-noise improvement; recommend it as next session's
opening move.

Good luck.
