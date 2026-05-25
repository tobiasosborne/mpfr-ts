# Handoff — 198 ports, 38% complete; binary-I/O ADR is next P1

You are picking up mpfr-ts after a triple-chunk continuation session:
- Worklog 019: closed `mpfr-ts-2ls` (value_codec strings); shipped 2 ports.
- Worklog 020 / Chunk 2: closed `mpfr-ts-3a9` (mpz API ADR); shipped 5 _z ports.
- Worklog 020 / Chunk 3: mega batch 3; shipped 17 of 30 (12 blocked at PREP triage, 1 dropped in calibration).

Total shipped this session: **24 ports**. State.db: **198 done · 27 blocked · 0 pending**.
Cumulative cost across all sessions to date: ~$15.

Three patterns worth knowing:

1. **Calibration-first discipline now caught 9 issues across 3 sessions** (017 scale2, 018 eq + 3 brokens, 019 fdump-trailing-zeros, 020 add_z/sub_z signed-zero + bs_1 cont=0 + free_pool codec gap + cbrt faithful-algorithm bug). The pattern holds: spend 10-30 min in calibration; catch what would have shipped as 'low-confidence-pass' noise.

2. **PREP-triage on a 30-fn batch correctly classified 12 as BLOCKED + 1 as DROP** before any PORT subagent was dispatched. Cost-saving: avoided ~$2-3 of wasted PORT work on functions that need ADR / PRNG / faithful-algorithm sessions.

3. **C-dispatch fidelity matters for signed-zero correctness.** Worklog 020 add_z trap (delegate-to-mpfr_add loses x's sign when z=0) → fixed by mirroring C's `_si` fast-path dispatch. Any future TS port that delegates to a sister arithmetic op MUST audit whether C has a dispatch table; not all delegations are equivalent.

## ⚠ Gotchas — read first

1. **`mpfr-ts-4h9` (binary I/O ADR for fpif_*) is now Priority 1.** Worklog 020 PREP triage blocked all 6 `mpfr_fpif_*` static helpers on this ADR. fpif is MPFR's binary serialization format. The 6 helpers mix buffer-writers (scaffoldable as Uint8Array codecs) and FILE*-readers (inherently file-based). One ADR covering the family is the right unit. **Estimated effort: 2-4 hours (binary format design + ADR + 1-2 reference ports).**

2. **`mpfr-ts-bpo` (PRNG ADR for random_deviate) is Priority 2.** Depends on `gmp_randstate_t` and 4 unported random_deviate helpers. TS substrate has no PRNG abstraction yet. Will unblock random_deviate_value + future erandom/nrandom. **Estimated effort: 1-3 hours (decide on a TS PRNG, write ADR, reference port).**

3. **`mpfr-ts-zhd` (cbrt Optimize) is Priority 4.** PREP-shipped a faithful integer-cube-root + Newton-bisect adjust at composite=0.11 (RNDN tie bug). Two paths in the issue: delegating shortcut via exp(ln(x)/3) OR faithful Ziv loop. Not blocking anything; can wait for the Optimize phase.

4. **`bd` doesn't auto-export to JSONL on manual commits.** Run `bd export -o .beads/issues.jsonl` before `git commit`. Tracked by `mpfr-ts-i8e`.

5. **Hex literal hygiene** — driver PRNG seed constants must be actual hex (0-9, A-F).

6. **`MPFR_ASSERTD` is debug-only** — never throw on debug-only assertions.

7. **C unsigned-arithmetic traps** in comparison functions (mpfr_eq n_bits=0 trap was caught in worklog 018; pattern holds for any `n - 1` underflow case).

8. **NEW (020): C-dispatch fidelity for signed-zero.** When porting `mpfr_<op>_z` / `mpfr_<op>_si` family functions: if the C source has `if (mpz_fits_slong_p(z)) return mpfr_<op>_si(...); else return foo(...);`, the TS port MUST mirror that dispatch (delegate to `mpfr_<op>_si` for the fast path). The lossless `mpfr_set_z + mpfr_<op>` path loses x's sign when x is ±0 because IEEE 754 sums (-0)+(+0) → +0, but C dispatches to `mpfr_<op>_si(x, 0, rnd)` which preserves x's sign.

9. **NEW (020): codec doesn't natively handle null/undefined scalar outputs.** Use `boolean` (true/false) as the success marker for void-returning ops in the TS port + golden_driver. mpfr-ts-2ls was the string version; null/undefined remains an open gap if needed in the future (probably file a P4 if a function legitimately needs nullable output).

## TL;DR — first 10 minutes

```bash
git pull --rebase
cat PHASE.md                                          # → Production
cat HANDOFF.md                                        # this file
cat docs/worklog/020-mpz-adr-and-mega-batch-3.md      # latest session
cat docs/worklog/019-value-codec-strings.md           # worklog 019
cat docs/adr/0003-mpz-api.md                          # new ADR
cat docs/adr/0002-approximation-helper-grading.md     # parking rules

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|27 done|198 pending|0

cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q   # 123 pass
bash eval/golden_master/build.sh                      # all drivers compile

# Smoke-check 3 representative ports from worklog 020:
for fn in mpfr_add_z mpfr_const_euler_bs_1 mpn_tdiv_qr; do
  case $fn in
    mpn_*) port=src/internal/mpn/${fn#mpn_}.ts ;;
    *) port=src/ops/${fn#mpfr_}.ts ;;
  esac
  bun eval/harness/runner.ts --function $fn --port $port \
    --golden eval/functions/$fn/golden.jsonl --output /tmp/v.json
done

bd ready                                              # 18 issues (3 new this session)
```

## Next-session priority sequence

### Priority 1: Resolve `mpfr-ts-4h9` (fpif_* binary I/O ADR)

**Why now**: 6 functions blocked; same shape as the mpfr-ts-2ls value_codec gap (one ADR + 1-2 reference ports unblocks the whole family). Most natural next chunk after the value_codec + mpz ADRs.

**Deliverable**: `docs/adr/0004-binary-io-api.md` + 1-2 reference ports demonstrating the chosen pattern (likely Uint8Array codecs for the 3 writer helpers; the 3 reader helpers may need a separate file-abstraction sub-decision).

**Estimated effort**: 2-4 hours.

### Priority 2: Resolve `mpfr-ts-bpo` (PRNG ADR)

**Why now**: blocks `mpfr_random_deviate_value` today and will block all future random fns (`mpfr_urandom`, `mpfr_erandom`, `mpfr_nrandom`, etc.) once the picker climbs to them.

**Deliverable**: ADR for a TS PRNG abstraction (likely: take `gmp_randstate` as an opaque interface backed by a userland PRNG like xoshiro256**) + reference port for `mpfr_random_deviate_value`.

**Estimated effort**: 1-3 hours.

### Priority 3: Next 25-30 port mega batch

Pending queue is empty after this session. Run `python3 eval/driver/ralph.py --next --batch-size 30` to surface the next tier. Per worklogs 017/018/020, cost is ~$3-5 per batch of 25-30 ports.

Same disciplines apply: serial dispatch, PREP triage (block early, drop in calibration if needed), calibration-first.

### Priority 4: Optimize-phase: `mpfr-ts-zhd` (cbrt)

Replace faithful PREP-broken reference with either delegating shortcut or a deliberate Ziv-loop session. Not blocking anything.

### Priority 5-N: Other open P3/P4 issues (carried)

- `mpfr-ts-4x5`, `mpfr-ts-e2n` — string-IO and printf API ADRs (related to but distinct from mpfr-ts-2ls which we resolved)
- `mpfr-ts-i8e` — git pre-commit hook for bd export
- `mpfr-ts-l4t` — AST gate require-core-import friction
- `mpfr-ts-ndc` — state.db port_path tmpdirs
- `mpfr-ts-18x`, `mpfr-ts-ai4`, `mpfr-ts-d6o`, `mpfr-ts-e4j`, `mpfr-ts-sr4` — harness polish
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg` — cleanup

None block scale-out.

## What's working now (don't change)

| Component | Path | Notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Strict equality; substrate exempt from requireCoreImport |
| **Value codec (UPDATED 019)** | `eval/harness/value_codec.ts` | String scalar passthrough + arm in compareOutput |
| **Golden master common.h (UPDATED 019)** | `eval/golden_master/common.h` | jl_output_scalar_str + jl_kv_str full JSON-escape |
| **Codec test (NEW 019)** | `eval/harness/value_codec.test.ts` | 7 bun tests for string passthrough |
| AST gate | `eval/harness/ast_check.ts` | Solid |
| Substrate (mpn) | `src/internal/mpn/` | 13 files (+`tdiv_qr.ts` worklog 020) |
| Substrate (mpfr) | `src/internal/mpfr/` | 12 files (`flags.ts` heavily used by flag-family ports) |
| Callgraph | `eval/driver/callgraph.json` | 525 fns |
| State DB | `eval/state.db` | 225 rows; 198 done, 27 blocked, 0 pending |
| ralph picker | `eval/driver/ralph.py --next` | seed + select |
| mutate.py | `eval/driver/mutate.py` | gate_status: killed/vacuous/survived/**low-confidence-pass**; carve-out validated in worklogs 017/018/020 |
| ADR 0001, 0002 | `docs/adr/` | Both load-bearing |
| **ADR 0003 (NEW 020)** | `docs/adr/0003-mpz-api.md` | bigint as mpz_t analogue |
| **NEW**: 24 worklog-019/020 ports | `src/ops/*` + `src/internal/mpn/tdiv_qr.ts` | All composite=1.0 |

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR.
- Modify ADR 0001/0002/0003 without writing a successor.
- Skip `bd export -o .beads/issues.jsonl` before `git commit`.
- Add dead code to port files to satisfy mutate.py.
- Use mnemonic letters in C hex literals (only 0-9, A-F).
- Run `ralph.py --parallel N` with N > 10.
- Manually port before generating the golden — let libmpfr tell you
  the actual contract first.
- Add `MPFR_ASSERTD`-as-throw validation in TS ports.
- **020**: Write narrow-perturbation broken reference ports. Prefer
  "collapse the entire decision tree to a constant output" — narrow
  perturbations leave too many cases passing and fail calibration.
- **020**: Naively delegate `mpfr_<op>_z` to `mpfr_set_z + mpfr_<op>`
  without mirroring C's `_si` fast-path dispatch. Signed-zero
  correctness depends on the dispatch.
- **020**: Edit existing src/ops/ ports as a side effect of working
  on a different task. Worklog 019 caught one drift incident
  (get_emin.ts comment deletion); preserve the discipline.

## Pickup-on-different-device checklist

1. `git clone git@github.com:tobiasosborne/cmpfr-ts.git` (or your fork)
2. `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. `curl -fsSL https://bun.sh/install | bash`
4. `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. `pip install pytest`
6. `bd bootstrap --yes && bd hooks install && bd import`
7. Smoke-check:
   - `cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q` # 123 pass
   - `bash eval/golden_master/build.sh` # all drivers compile
   - 3 representative worklog-020 ports grade composite=1.0 (per TL;DR loop).
8. Read CLAUDE.md → this file → worklog 020 → 019 → 018 → 017 → 016 → 015 → 014 → ADR 0001/0002/0003.

## Open bd issues at session end (18 total — 3 new, 2 closed)

P1:
- **NEW** `mpfr-ts-4h9` — **PRIORITY 1**: binary I/O ADR for fpif_* (6 blocked)

P2:
- **NEW** `mpfr-ts-bpo` — PRNG ADR for random_deviate (1 blocked, more accumulating)
- **NEW** `mpfr-ts-zhd` — cbrt Optimize phase (1 blocked)

P3:
- `mpfr-ts-4x5`, `mpfr-ts-e2n` — string-IO and printf API ADRs
- `mpfr-ts-i8e` — git pre-commit hook
- `mpfr-ts-ndc` — state.db port_path tmpdirs
- `mpfr-ts-18x`, `mpfr-ts-ai4`, `mpfr-ts-d6o`, `mpfr-ts-e4j`, `mpfr-ts-sr4`

P4:
- `mpfr-ts-l4t` — AST gate require-core-import friction
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

**Closed this session**: `mpfr-ts-2ls`, `mpfr-ts-3a9`.

## One final thing

Library is now **198 / 525 = 38% complete**. Three full sessions of mega-batch cadence have shipped 76 ports for ~$15 total. The PREP-PORT cost shape is stable; the calibration discipline is mature; the carve-out predicate has handled 3 consecutive batches with zero 'survived' false carves.

Two ADR sessions are now the bottleneck (binary I/O + PRNG). Both are scoped at 2-4 hours each. After they land, the next 50-100 ports should flow with the same cadence.

Good luck.
