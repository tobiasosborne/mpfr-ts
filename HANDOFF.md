# Handoff -- 219 ports, 41.7% complete; Flash is the PORTER now

You are picking up mpfr-ts after a two-chunk continuation session
(worklog 022):
- Chunk 1 (Phases 0-5): integrated DeepSeek-V4-Flash via opencode as
  the PORT-step porter. New files: `eval/driver/opencode_runner.py`,
  `eval/driver/run_deepseek_port.py`. `ralph.py --grade` now accepts
  `--model` / `--effort` / `--usd-est` (backward-compat preserved).
- Chunk 2 (Phase 6): first production Flash/L3 batch. 10 picks, 8
  portable, 8 shipped. Phase 6 Flash cost ~$0.05 + ~5 min wall at
  parallel-2.

Total shipped this session: **8 ports**. State.db: **219 done . 24
blocked . 2 pending** (printf, rand_raw -- both ADR-blocked at PREP,
parents `mpfr-ts-e2n` and `mpfr-ts-bpo` respectively).
Cumulative cost across all sessions to date: ~$21-22.

Three patterns worth knowing:

1. **DeepSeek-Flash is now the PORTER (sonnet stays the PREPPER).**
   Per `auto-port-eval/RESULTS_DEEPSEEK.md` (n=150) Flash@L3 sits
   alone on the cost-Pareto frontier: $0.005/port at 0.999 mean
   grade, 9x cheaper than sonnet/L3 at equal quality. Drive ports
   with `python3 eval/driver/run_deepseek_port.py --fn <name>` after
   PREP has produced `/tmp/eval_<fn>/PROMPT.md`. Grade with
   `python3 eval/driver/ralph.py --grade --model
   deepseek-anthropic/deepseek-v4-flash --fn <name>` -- cost auto-loads
   from `cost.json`.

2. **Calibration-first discipline caught 3 issues this session.**
   (a) `mpfr_print_mant_binary` golden_driver used MPFR-private
   macros not in public `mpfr.h`; fixed by inlining DRV_MANT /
   DRV_PREC / drv_setmax shims. (b) `mpfr_set_sj/spec.json` had
   `class:"conversion"` -- runner rejects; fixed to `"misc"` per the
   established convention. (c) `mpfr_mpz_init` reference port had no
   core.ts import -- Law 4 AST gate correctly rejected; fixed with
   the standard `import { MPFRError as _MPFRError } ...; void _MPFRError`
   pattern for vacuous zero-arg factories.

3. **Mutate gate continues to validate calibration discipline.** 8/8
   gated this session: 6 killed + 1 vacuous + 1 low-confidence-pass.
   0 survived. Carve-out predicate has now handled 5 consecutive
   batches with zero false carves.

## [!] Gotchas -- read first

1. **NEW (022): DeepSeek-Flash is the PORTER.** PREP stays sonnet.
   PORT runs as `python3 eval/driver/run_deepseek_port.py --fn <name>`.
   Grade with `ralph.py --grade --model
   deepseek-anthropic/deepseek-v4-flash --fn <name>`. The cost auto-loads
   from `/tmp/eval_<fn>/cost.json`. See worklog 022 for the full
   integration story.

2. **NEW (022): Don't disable the safe-Unicode normalize in
   `run_deepseek_port.py`.** Flash occasionally emits Unicode arrows
   (U+2192 for `->`, etc.) in comments. The normalize covers the 4
   common arrows (`->`, `<-`, `=>`, `<=`) plus other safe glyphs.
   Without it, Rule 13's ASCII guard correctly rejects ~1 in 10
   Flash ports. The normalize is **safe** (renders identically in
   editors) but **load-bearing** (without it you pay ~$0.005/fn in
   wasted re-runs).

3. **NEW (022): `ralph.py --ship` does NOT yet thread `--model`.**
   The re-grade that `--ship` runs for confirmation always writes a
   duplicate `runs` row as `sonnet/L3/0.0`, even for Flash-ported
   fns. Correctness and cost on the canonical row from
   `ralph.py --grade` are still right; the duplicate is aesthetic
   only but pollutes dashboards. **Priority 1.5** to fix
   (~15 min, thread the existing flags through).

4. **Next mega batch via Flash.** Pending queue: 2 PREP-blocked
   (printf, rand_raw -- both ADR-bound). Run `python3
   eval/driver/ralph.py --next --batch-size 25` for the next tier.
   Use the Phase 6 process from worklog 022: PREP sonnet subagent,
   parallel-2 Flash PORT, calibration + grade + mutate + ship.

5. **`mpfr-ts-75v` -- opencode cold-start latency variance.** One
   12-min hang observed during Phase 4 smoke. Phase 6 ran
   parallel-2 (not the auto-port-eval-validated parallel-8) as a
   defensive measure; no recurrence across ~12 Flash invocations.
   Worth monitoring; not blocking.

6. **`mpfr-ts-8qy` (mpq API ADR) is Priority 2.** Unblocks
   `mpfr_get_q` + downstream `_q` family (likely 5-8 fns). Mirror
   ADR 0003 structure. **Estimated effort: 1-3 hours.**

7. **`mpfr-ts-bpo` (PRNG ADR) is Priority 2.** Blocks `mpfr_rand_raw`
   today (now in `pending`) and the `random_deviate` family. Will
   unblock `mpfr_urandom`, `mpfr_erandom`, `mpfr_nrandom`.
   **Estimated effort: 1-3 hours.**

8. **`mpfr-ts-e2n` (printf API ADR) is Priority 3.** Blocks
   `mpfr_printf` today (now in `pending`). Needs a TS-native format
   API decision.

9. **`mpfr-ts-1ts` (logging API ADR) is Priority 3.** Not blocking
   anything urgent.

10. **`mpfr-ts-2wd` (park init_cache) is Priority 4.** 5-min close-out
    under ADR 0002.

11. **`bd` doesn't auto-export to JSONL on manual commits.** Run
    `bd export -o .beads/issues.jsonl` before `git commit`. Tracked by
    `mpfr-ts-i8e`.

12. **Hex literal hygiene** -- driver PRNG seed constants must be
    actual hex (0-9, A-F).

13. **`MPFR_ASSERTD` is debug-only** -- never throw on debug-only
    assertions.

14. **Codec output-collision via `kind` field** (from 021). Any port
    whose return object has a `kind` field equal to one of `{normal,
    zero, inf, nan}` but lacks `prec` will (since 021) correctly fall
    through to generic-struct decoding. If you add a new port output
    type, prefer distinctive field names to avoid the collision.

15. **Promote-step sed covers `src/ops/` cross-imports** (from 021).
    Both `../../../src/core.ts -> ../core.ts` AND `../../../src/ops/
    -> ./` must be applied. Common cross-imports between sister
    functions in `src/ops/`.

16. **Rule 7 vacuous-function relaxation** (from 021). Vacuous fns
    cannot meet the happy>=20 / edge>=30 minimums. The runner allows
    this; enforcement open as `mpfr-ts-sr4`. spec.json `note` should
    explain why coverage is minimal.

17. **ADR 0004 substrate uses `Uint8Array`** (from 021). Never
    `Buffer` or `ArrayBuffer`. The codec encodes byte buffers as
    decimal-bigint scalars (LE-uint) on the wire.

18. **C-dispatch fidelity for signed-zero** (from 020). When porting
    `mpfr_<op>_z` / `mpfr_<op>_si`, mirror C's `mpz_fits_slong_p`
    fast path. The lossless `mpfr_set_z + mpfr_<op>` path loses x's
    sign when x is +/-0.

19. **Codec doesn't natively handle null/undefined scalar outputs**
    (from 020). Use `boolean` (true/false) as the success marker.

20. **Vacuous AST-gate workaround** (this session). For zero-arg
    vacuous factories whose body genuinely doesn't reference any
    core.ts type, satisfy Law 4 with:
    ```ts
    import { MPFRError as _MPFRError } from '/.../src/core.ts';
    void _MPFRError;
    ```
    The absolute path gets rewritten on ship.

## TL;DR -- first 10 minutes

```bash
git pull --rebase
cat PHASE.md                                          # -> Production
cat HANDOFF.md                                        # this file
cat docs/worklog/022-deepseek-flash-integration-and-batch.md   # latest
cat docs/worklog/021-adr-0004-and-mega-batch-4.md     # prior session
cat docs/adr/0004-binary-io-api.md                    # binary I/O ADR
cat docs/adr/0003-mpz-api.md                          # mpz ADR
cat docs/adr/0002-approximation-helper-grading.md     # parking rules
cat ../auto-port-eval/RESULTS_DEEPSEEK.md             # Flash justification

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|24 done|219 pending|2

cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q   # 123 pass
bash eval/golden_master/build.sh                      # all drivers compile

# Smoke-check 3 representative ports from worklog 022:
for fn in mpfr_set_sj mpfr_mpz_set_uj mpfr_print_mant_binary; do
  short=${fn#mpfr_}
  bun eval/harness/runner.ts --function $fn --port src/ops/$short.ts \
    --golden eval/functions/$fn/golden.jsonl --output /tmp/v.json
done

bd ready                                              # 22 issues (3 new, 2 closed)
python3 eval/driver/ralph.py --next --batch-size 25   # surfaces next tier

# NEW (022): drive a Flash port end-to-end (after PREP)
python3 eval/driver/run_deepseek_port.py --fn <name>
python3 eval/driver/ralph.py --grade \
    --model deepseek-anthropic/deepseek-v4-flash \
    --effort L3 --fn <name>
```

## Next-session priority sequence

### Priority 1: Run the next mega batch via Flash

Pending queue has 2 ADR-blocked (printf, rand_raw); next `--next`
will surface the next tier. Per worklog 022, cost shape is now
~$1.5-2 per batch of 25-30 ports (PREP sonnet dominates; Flash PORT
is negligible at ~$0.005/fn).

Use the Phase 6 process: PREP sonnet subagent for triage + spec +
goldens + brokens; parallel-2 Flash PORT (defensive vs cold-start
variance); calibration + grade + mutate + ship inline.

### Priority 1.5: Thread `--model` through `ralph.py --ship`

`--ship` re-grades each port for confirmation. The re-grade path
doesn't honor `--model` so every Flash port currently gets a
duplicate `runs` row recorded as sonnet/0.0. Aesthetic only --
canonical row is correct -- but worth fixing for clean dashboards.

**Estimated effort: 15 min.** Pattern: copy the three flags from
the `--grade` argparse block to `--ship`, forward them into the
internal `run_grade` call inside `_promote_port`.

### Priority 2: Resolve `mpfr-ts-8qy` (mpq API ADR)

Unblocks `mpfr_get_q` + downstream `_q` family. Mirror ADR 0003.

**Deliverable**: `docs/adr/0005-mpq-api.md` + 1-2 reference ports.

**Estimated effort: 1-3 hours.**

### Priority 2: Resolve `mpfr-ts-bpo` (PRNG ADR)

Blocks `mpfr_rand_raw` today (now `pending`) and the
`random_deviate` family. Will unblock `mpfr_urandom`, etc.

**Deliverable**: ADR for a TS PRNG abstraction + reference port for
`mpfr_random_deviate_value`.

**Estimated effort: 1-3 hours.**

### Priority 3: Logging + printf + parking + cbrt cleanups

- `mpfr-ts-e2n` -- printf API ADR (blocks `mpfr_printf` today)
- `mpfr-ts-1ts` -- logging API ADR
- `mpfr-ts-2wd` -- park `mpfr_init_cache` (5 min)
- `mpfr-ts-zhd` -- cbrt Optimize phase (no urgency)

### Priority 4-N: Other open P3/P4 issues (carried)

- `mpfr-ts-4x5` -- string-IO API ADR
- `mpfr-ts-i8e` -- git pre-commit hook for bd export
- `mpfr-ts-ndc` -- state.db port_path tmpdirs
- `mpfr-ts-18x`, `mpfr-ts-d6o`, `mpfr-ts-sr4` -- harness polish
- `mpfr-ts-75v` -- opencode cold-start variance (monitor)

None block scale-out.

## What's working now (don't change)

| Component | Path | Notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Strict equality; substrate exempt from requireCoreImport |
| Value codec | `eval/harness/value_codec.ts` | `looksLikeMpfrWire` requires `prec` (021 tightening); unchanged this session |
| Golden master common.h | `eval/golden_master/common.h` | jl_output_scalar_str + jl_kv_str full JSON-escape |
| AST gate | `eval/harness/ast_check.ts` | Solid |
| Substrate (mpn) | `src/internal/mpn/` | 13 files |
| Substrate (mpfr) | `src/internal/mpfr/` | 12 files (`flags.ts` heavily used by flag-family ports) |
| Callgraph | `eval/driver/callgraph.json` | 525 fns |
| State DB | `eval/state.db` | 245 rows; 219 done, 24 blocked, 2 pending |
| ralph picker | `eval/driver/ralph.py --next` | seed + select |
| ralph grader (UPDATED 022) | `eval/driver/ralph.py --grade` | Now accepts `--model` / `--effort` / `--usd-est`; cost.json auto-load for deepseek models |
| **NEW (022): opencode wrapper** | `eval/driver/opencode_runner.py` | Spawns opencode CLI, streams JSON events, surfaces tokens on exit |
| **NEW (022): Flash PORT driver** | `eval/driver/run_deepseek_port.py` | Cyrillic + Write-tool guards, safe-Unicode normalize (covers `->`, `<-`, `=>`, `<=`), Flash pricing cost.json emit |
| mutate.py | `eval/driver/mutate.py` | gate_status: killed/vacuous/survived/low-confidence-pass; carve-out validated across 5 batches |
| ADR 0001-0004 | `docs/adr/` | All load-bearing |
| **NEW**: 8 worklog-022 ports | `src/ops/*` | All composite=1.0 (Flash-ported) |

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR.
- Modify ADR 0001/0002/0003/0004 without writing a successor.
- Skip `bd export -o .beads/issues.jsonl` before `git commit`.
- Add dead code to port files to satisfy mutate.py.
- Use mnemonic letters in C hex literals (only 0-9, A-F).
- Run `ralph.py --parallel N` with N > 10.
- Run Flash PORT at parallel > 2 until `mpfr-ts-75v` is understood.
- Manually port before generating the golden -- let libmpfr tell you
  the actual contract first.
- Add `MPFR_ASSERTD`-as-throw validation in TS ports.
- Write narrow-perturbation broken reference ports. Prefer "collapse
  the entire decision tree to a constant output" (HANDOFF gotcha #10).
- Naively delegate `mpfr_<op>_z` to `mpfr_set_z + mpfr_<op>` without
  mirroring C's `_si` fast-path dispatch (signed-zero correctness).
- Edit existing src/ops/ ports as a side effect of working on a
  different task.
- Import `node:fs` / `node:fs/promises` / `Bun.file` in any `src/`
  file (ADR 0004 Invariant 4).
- Forget the cross-import sed rule when promoting reference ports.
  Both core.ts AND src/ops/ rewrites must apply.
- **NEW (022): Disable the safe-Unicode normalize in
  `run_deepseek_port.py`.** It's what makes Flash output ship-able
  without re-runs. Adding glyphs is fine; removing the existing 4
  arrows (`->`, `<-`, `=>`, `<=`) is not.
- **NEW (022): Use `--model` with `ralph.py --ship` and expect it
  to be honored.** Known gap. The `--ship` re-grade always writes a
  sonnet/L3/0.0 duplicate row regardless of how the original port
  was produced. Priority 1.5 to fix.

## Pickup-on-different-device checklist

1. `git clone git@github.com:tobiasosborne/cmpfr-ts.git` (or your fork)
2. `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. `curl -fsSL https://bun.sh/install | bash`
4. `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. `pip install pytest`
6. `bd bootstrap --yes && bd hooks install && bd import`
7. **NEW (022)**: install opencode CLI (see `opencode_runner.py`
   docstring) and set the deepseek-anthropic provider API key.
8. Smoke-check:
   - `cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q` # 123 pass
   - `bash eval/golden_master/build.sh` # all drivers compile
   - 3 representative worklog-022 ports grade composite=1.0 (per TL;DR loop).
9. Read CLAUDE.md -> this file -> worklog 022 -> 021 -> 020 -> 019 ->
   ADR 0001-0004 -> auto-port-eval/RESULTS_DEEPSEEK.md.

## Open bd issues at session end (22 total -- 3 new, 2 closed)

P1: (cleared)

P2:
- **NEW** `mpfr-ts-75v` -- opencode cold-start latency variance
- `mpfr-ts-8qy` -- mpq API ADR (unblocks `mpfr_get_q` + `_q` family)
- `mpfr-ts-bpo` -- PRNG ADR for random_deviate
- `mpfr-ts-i8e` -- git pre-commit hook
- `mpfr-ts-ra3` -- cbrt block (duplicate-ish of zhd)
- `mpfr-ts-zhd` -- cbrt Optimize phase

P3:
- `mpfr-ts-1ts` -- logging API ADR for MPFR_LOG_* facility
- `mpfr-ts-4x5`, `mpfr-ts-e2n` -- string-IO and printf API ADRs
- `mpfr-ts-ndc` -- state.db port_path tmpdirs
- `mpfr-ts-18x`, `mpfr-ts-d6o`, `mpfr-ts-sr4` -- harness polish

P4:
- `mpfr-ts-2wd` -- park `mpfr_init_cache`
- `mpfr-ts-l4t` -- AST gate require-core-import friction
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

**Closed this session**: `mpfr-ts-9m7` (Flash integration epic);
`mpfr-ts-alp` (false alarm on Flash abs-paths).

## One final thing

Library is now **219 / 525 = 41.7% complete**. Five sessions of
mega-batch cadence have shipped 97 ports for ~$22 total. This
session marks the first with DeepSeek-Flash as the PORTER: cost
shape has shifted qualitatively (PORT ~$0.005/fn vs sonnet's
~$0.05/fn) but correctness is unchanged across 11 Flash ports (3
smoke + 8 batch, all composite=1.0). PREP sonnet now dominates the
cost shape at ~$0.15/fn; orchestrator overhead a similar share.

The PREP-PORT split continues to validate: every calibration issue
caught (3 this session) was a PREP-step omission or
infrastructure-edge case, not a Flash failure. The mutate gate
remains the floor on correctness signal -- 0 survived across 8
Flash ports despite the porter switch.

After 022, the bottleneck is no longer PORT cost. It's PREP-step
sonnet cost and orchestrator overhead. The next batch should flow
at the established ~$1.5-2 / 25-30 fns cadence. Two follow-up ADRs
(mpq, PRNG) are 1-3 hours each and don't gate the next mega batch.

Good luck.
