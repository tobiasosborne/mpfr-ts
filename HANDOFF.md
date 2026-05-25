# Handoff -- 211 ports, 40% complete; next batch + mpq ADR are the P1/P2

You are picking up mpfr-ts after a two-chunk continuation session
(worklog 021):
- Chunk 1: closed `mpfr-ts-4h9` (binary I/O ADR); shipped 6 fpif statics + ADR 0004.
- Chunk 2: 10-fn mega batch; shipped 7, blocked 3 at PREP (filed `mpfr-ts-8qy`, `mpfr-ts-1ts`, `mpfr-ts-2wd`).

Total shipped this session: **13 ports**. State.db: **211 done . 24 blocked . 0 pending**.
Cumulative cost across all sessions to date: ~$18-19.

Three patterns worth knowing:

1. **Calibration-first discipline caught 2 more issues this session.**
   (a) A codec collision: substrate outputs with a `kind` field carrying
   `MPFR['kind']` values (fpif_read_exponent's `{kind, sign, exp, nextPos}`)
   were wrongly routed to `decodeMpfr`. Fixed by tightening
   `looksLikeMpfrWire` to require `typeof r['prec'] === 'string'`.
   (b) Cross-import sed in the promote step missed `../../../src/ops/`.
   Fixed inline with a second sed rule.

2. **PREP-triage works ruthlessly on 10-fn batches.** 3 of 10 fns
   correctly classified as ADR-needed before any PORT was dispatched.
   Cost-saving: ~$0.50-1 of wasted PORT work avoided.

3. **The mutate gate continues to validate the calibration discipline.**
   13/13 functions gated this session: 9 killed + 2 vacuous + 2
   trivial-killed. 0 survived. Carve-out predicate has now handled
   4 consecutive batches with zero false carves.

## [!] Gotchas -- read first

1. **Next priority is the next mega batch (queue empty).** Run
   `python3 eval/driver/ralph.py --next --batch-size 25` to surface
   the next tier. Three sessions of 10-30-fn batches have shipped at
   ~$3-5 each.

2. **`mpfr-ts-8qy` (mpq API ADR) is Priority 2.** Unblocks `mpfr_get_q`
   and the entire `_q` family (likely 5-8 downstream fns). Two viable
   surfaces: `{num: bigint, den: bigint}` value tuple vs. fraction-string
   codec. Mirror ADR 0003 structure. **Estimated effort: 1-3 hours.**

3. **`mpfr-ts-bpo` (PRNG ADR for random_deviate) is Priority 3.**
   Depends on `gmp_randstate_t` and 4 unported random_deviate helpers.
   Will unblock `random_deviate_value` + future `mpfr_urandom`,
   `mpfr_erandom`, `mpfr_nrandom`. **Estimated effort: 1-3 hours.**

4. **`mpfr-ts-1ts` (logging API ADR) is Priority 4.** C body is
   `static __attribute__((constructor))` inside `#ifdef MPFR_USE_LOGGING`,
   reads 7+ env vars. Likely path: TS-level callback registry or
   no-op stub. Not blocking anything urgent.

5. **`mpfr-ts-2wd` (park init_cache) is Priority 4.** Trivial close-out
   under ADR 0002; takes 5 minutes.

6. **`mpfr-ts-zhd` (cbrt Optimize) is Priority 5.** PREP-shipped a
   faithful integer-cube-root + Newton-bisect adjust at composite=0.11
   (RNDN tie bug). Not blocking anything; can wait for the Optimize
   phase.

7. **`bd` doesn't auto-export to JSONL on manual commits.** Run
   `bd export -o .beads/issues.jsonl` before `git commit`. Tracked by
   `mpfr-ts-i8e`.

8. **Hex literal hygiene** -- driver PRNG seed constants must be actual
   hex (0-9, A-F).

9. **`MPFR_ASSERTD` is debug-only** -- never throw on debug-only
   assertions.

10. **NEW (021): codec output-collision via `kind` field.** Any future
    TS port whose return object has a `kind` field equal to one of
    `{normal, zero, inf, nan}` but lacks `prec` would have been mis-routed
    by the harness pre-021. The post-021 codec narrows correctly. If you
    are adding a new port output type, make sure the wire shape can't
    be mistaken for an MPFR value -- pick distinctive field names if in
    doubt.

11. **NEW (021): promote-step sed must cover `src/ops/` cross-imports.**
    When promoting a reference port to `src/ops/<short>.ts`, run BOTH:
    - `sed 's|../../../src/core.ts|../core.ts|g'`
    - `sed 's|../../../src/ops/|./|g'`
    Cross-imports between sister functions in `src/ops/` are common
    (`set_sj_2exp` -> `set_uj_2exp`, `modf` -> `set` + `rint_trunc` +
    `frac`). The first sed is necessary; the second covers the
    cross-imports.

12. **NEW (021): Rule 7 vacuous-function relaxation.** Vacuous fns
    (e.g. `mpfr_get_version`, `mpfr_mpz_clear`) cannot meet Rule 7's
    happy>=20 / edge>=30 etc. minimums because they have no algorithm
    to fuzz. The runner currently allows this (Rule 7 enforcement is
    open as `mpfr-ts-sr4`). For these, the spec.json `note` field
    should explain why coverage is minimal.

13. **NEW (021): ADR 0004 substrate uses `Uint8Array`, not `Buffer` or
    `ArrayBuffer`.** Future ports involving binary data must use the
    same primitive. The codec encodes byte buffers as decimal-bigint
    scalars (LE-uint interpretation) on the wire.

14. **C-dispatch fidelity for signed-zero** (from 020): When porting
    `mpfr_<op>_z` / `mpfr_<op>_si` family functions, mirror C's
    `mpz_fits_slong_p` fast path. The lossless
    `mpfr_set_z + mpfr_<op>` path loses x's sign when x is +/-0.

15. **codec doesn't natively handle null/undefined scalar outputs** (from 020):
    Use `boolean` (true/false) as the success marker.

## TL;DR -- first 10 minutes

```bash
git pull --rebase
cat PHASE.md                                          # -> Production
cat HANDOFF.md                                        # this file
cat docs/worklog/021-adr-0004-and-mega-batch-4.md     # latest session
cat docs/worklog/020-mpz-adr-and-mega-batch-3.md      # prior session
cat docs/adr/0004-binary-io-api.md                    # new ADR (binary I/O)
cat docs/adr/0003-mpz-api.md                          # mpz ADR
cat docs/adr/0002-approximation-helper-grading.md     # parking rules

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|24 done|211 pending|0

cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q   # 123 pass
bash eval/golden_master/build.sh                      # all drivers compile

# Smoke-check 3 representative ports from worklog 021:
for fn in mpfr_fpif_store_precision mpfr_modf mpfr_inits; do
  short=${fn#mpfr_}
  bun eval/harness/runner.ts --function $fn --port src/ops/$short.ts \
    --golden eval/functions/$fn/golden.jsonl --output /tmp/v.json
done

bd ready                                              # 19 issues (3 new this session, 1 closed)
python3 eval/driver/ralph.py --next --batch-size 25   # surfaces next tier
```

## Next-session priority sequence

### Priority 1: Run the next mega batch

Pending queue is empty. `ralph.py --next --batch-size 25` will surface
the next ~25 portable fns. Per worklogs 017/018/020/021, cost is ~$3-5
per batch of 25-30 ports.

Same disciplines apply: serial dispatch, PREP triage (block early,
drop in calibration if needed), calibration-first.

### Priority 2: Resolve `mpfr-ts-8qy` (mpq API ADR)

**Why now**: unblocks `mpfr_get_q` and the entire `_q` family (5-8
fns). Pattern mirrors ADR 0003 (mpz API): native TS primitive as the
analogue.

**Deliverable**: `docs/adr/0005-mpq-api.md` + 1-2 reference ports
demonstrating the chosen pattern.

**Estimated effort**: 1-3 hours.

### Priority 3: Resolve `mpfr-ts-bpo` (PRNG ADR)

**Why now**: blocks `mpfr_random_deviate_value` today and will block
all future random fns (`mpfr_urandom`, `mpfr_erandom`, `mpfr_nrandom`,
etc.) once the picker climbs to them.

**Deliverable**: ADR for a TS PRNG abstraction (likely: opaque
interface backed by a userland PRNG like xoshiro256**) + reference
port for `mpfr_random_deviate_value`.

**Estimated effort**: 1-3 hours.

### Priority 4: Logging + parking + cbrt cleanups

- `mpfr-ts-1ts` -- logging API ADR
- `mpfr-ts-2wd` -- park `mpfr_init_cache` (5 min)
- `mpfr-ts-zhd` -- cbrt Optimize phase (no urgency)

### Priority 5-N: Other open P3/P4 issues (carried)

- `mpfr-ts-4x5`, `mpfr-ts-e2n` -- string-IO and printf API ADRs
- `mpfr-ts-i8e` -- git pre-commit hook for bd export
- `mpfr-ts-ndc` -- state.db port_path tmpdirs
- `mpfr-ts-18x`, `mpfr-ts-d6o`, `mpfr-ts-sr4` -- harness polish

None block scale-out.

## What's working now (don't change)

| Component | Path | Notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Strict equality; substrate exempt from requireCoreImport |
| **Value codec (UPDATED 021)** | `eval/harness/value_codec.ts` | `looksLikeMpfrWire` now requires `prec` field to avoid kind-field collision with non-MPFR substrate outputs |
| Golden master common.h | `eval/golden_master/common.h` | jl_output_scalar_str + jl_kv_str full JSON-escape (021 fpif batch reused as-is) |
| AST gate | `eval/harness/ast_check.ts` | Solid |
| Substrate (mpn) | `src/internal/mpn/` | 13 files |
| Substrate (mpfr) | `src/internal/mpfr/` | 12 files (`flags.ts` heavily used by flag-family + inexflag_p ports) |
| Callgraph | `eval/driver/callgraph.json` | 525 fns |
| State DB | `eval/state.db` | 235 rows; 211 done, 24 blocked, 0 pending |
| ralph picker | `eval/driver/ralph.py --next` | seed + select |
| mutate.py | `eval/driver/mutate.py` | gate_status: killed/vacuous/survived/low-confidence-pass; carve-out validated across 4 batches |
| ADR 0001, 0002, 0003 | `docs/adr/` | All load-bearing |
| **ADR 0004 (NEW 021)** | `docs/adr/0004-binary-io-api.md` | Uint8Array + (buffer, position) pure-function substrate; no node:fs in src/ |
| **NEW**: 13 worklog-021 ports | `src/ops/*` | All composite=1.0 |

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR.
- Modify ADR 0001/0002/0003/0004 without writing a successor.
- Skip `bd export -o .beads/issues.jsonl` before `git commit`.
- Add dead code to port files to satisfy mutate.py.
- Use mnemonic letters in C hex literals (only 0-9, A-F).
- Run `ralph.py --parallel N` with N > 10.
- Manually port before generating the golden -- let libmpfr tell you
  the actual contract first.
- Add `MPFR_ASSERTD`-as-throw validation in TS ports.
- Write narrow-perturbation broken reference ports. Prefer "collapse
  the entire decision tree to a constant output" (HANDOFF gotcha #10).
- Naively delegate `mpfr_<op>_z` to `mpfr_set_z + mpfr_<op>` without
  mirroring C's `_si` fast-path dispatch (signed-zero correctness).
- Edit existing src/ops/ ports as a side effect of working on a
  different task.
- **021**: Import `node:fs` / `node:fs/promises` / `Bun.file` in any
  `src/` file. The fpif ports keep all binary I/O at the
  `Uint8Array` boundary; users compose with their runtime's file API
  at the call site (per ADR 0004 Invariant 4).
- **021**: Forget the cross-import sed rule when promoting reference
  ports to `src/ops/<short>.ts`. Both `../../../src/core.ts -> ../core.ts`
  AND `../../../src/ops/ -> ./` must be applied.

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
   - 3 representative worklog-021 ports grade composite=1.0 (per TL;DR loop).
8. Read CLAUDE.md -> this file -> worklog 021 -> 020 -> 019 -> 018 -> ADR 0001-0004.

## Open bd issues at session end (21 total -- 3 new, 1 closed)

P1: (cleared by this session)

P2:
- **NEW** `mpfr-ts-8qy` -- mpq API ADR (unblocks `mpfr_get_q` + downstream `_q` family)
- `mpfr-ts-bpo` -- PRNG ADR for random_deviate
- `mpfr-ts-i8e` -- git pre-commit hook
- `mpfr-ts-ra3` -- cbrt block (duplicate-ish of zhd)
- `mpfr-ts-zhd` -- cbrt Optimize phase

P3:
- **NEW** `mpfr-ts-1ts` -- logging API ADR for MPFR_LOG_* facility
- `mpfr-ts-4x5`, `mpfr-ts-e2n` -- string-IO and printf API ADRs
- `mpfr-ts-ndc` -- state.db port_path tmpdirs
- `mpfr-ts-18x`, `mpfr-ts-d6o`, `mpfr-ts-sr4` -- harness polish

P4:
- **NEW** `mpfr-ts-2wd` -- park `mpfr_init_cache`
- `mpfr-ts-l4t` -- AST gate require-core-import friction
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

**Closed this session**: `mpfr-ts-4h9` (binary I/O ADR).

## One final thing

Library is now **211 / 525 = 40% complete**. Four sessions of mega-batch
cadence have shipped 89 ports for ~$19 total. The PREP-PORT cost shape
is stable; the calibration discipline catches what the goldens would
have shipped; the carve-out predicate has handled 4 consecutive batches
with zero 'survived' false carves.

After 021, the bottleneck shifts from ADR-blocked (was fpif) to
pure-batch throughput. The next 25-30 ports should flow at the
established ~$3-5 / batch / 25-30 fns cadence. Two follow-up ADRs
(mpq, PRNG) are 1-3 hours each and don't gate the next mega batch.

Good luck.
