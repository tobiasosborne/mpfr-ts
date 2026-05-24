# Handoff — 85 ports landed; scale-out engine built and validated

You are picking up mpfr-ts after a session that took the port from 50 to
**85** (state.db `done=85`, 4 cleanly blocked, all composite=1.0 against
libmpfr-derived goldens). The scale-out engine described in
`docs/worklog/005-scale-out-handoff.md` is now built, tested, and validated;
the mega-batch pattern (one opus prep + 15 parallel sonnets + atomic
--ship) hits 0.33 orchestrator-actions per function.

## TL;DR — where to start

```bash
# 1. Pick up state
cat PHASE.md                                       # → Pilot
cat docs/worklog/006-scale-out-engine.md           # this session's worklog (your contract)
bun x tsc --noEmit                                  # must be clean
bun eval/acceptance/step5/run.ts                    # 5/5 expected
/home/tobias/.local/bin/pytest eval/driver/tests/   # 52/52 expected (38 ralph + 14 callgraph)
sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|4, done|85

# 2. Read 006-scale-out-engine.md cover-to-cover. It's the live spec for
# anything you do next.

# 3. Read CLAUDE.md, PHASE.md, then the worklog stack (001 → 006).
```

## What works (don't change)

| component | path | notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen; never modify without ADR |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Solid (1287 LOC; `mpfr-ts-5a3` for golf) |
| Codec | `eval/harness/value_codec.ts` | One known gap: scalar strings (`mpfr-ts-i5z`) |
| AST gate | `eval/harness/ast_check.ts` | Import-strip fix landed this session |
| Wire helpers | `eval/golden_master/common.h` | Solid |
| Substrate | `src/internal/{mpn,mpfr}/` | 9 files (+ 3 this session: powerof2_raw2/raw, round_p) |
| Prompt renderer | `eval/driver/prompts.py` | Unchanged from 005 |
| Callgraph | `eval/driver/callgraph.py` | New this session; 525 fns extracted |
| Driver | `eval/driver/ralph.py` | 5 modes; +800 LOC this session |
| State DB | `eval/state.db` | 89 rows; 85 done, 4 blocked |

## The 85 ports (alphabetical, public surface)

Substrate (10): `mpn_add_n`, `mpn_cmp`, `mpn_lshift`, `mpn_sub_n`, plus 6
under `src/internal/mpfr/`: `round_raw`, `cmp_raw`, `powerof2_raw2`,
`powerof2_raw`, `round_p`.

Public ops (75): all the original 46 from 004 plus this session's:
`abort_prec_max` (stub, blocked), `add1sp1`, `add1sp1n`, `add1sp2`,
`add1sp2n`, `add1sp3`, `add_si`, `add_ui`, `check_range`, `dim`, `div_2ui`,
`get_exp`, `get_prec`, `init`, `mul_2ui`, `mul_si`, `mul_ui`, `overflow`,
`print_rnd_mode`, `set`, `set4`, `set_exp`, `setmax`, `setmin`, `sub_si`,
`sub_ui`, `sub1sp1`, `sub1sp1n`, `sub1sp2`, `sub1sp2n`, `sub1sp3`, `swap`,
`underflow`.

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR.
- Flip `PHASE.md` from `Pilot` to `Production` without writing
  `docs/worklog/NNN-phase-transition.md` first (CLAUDE.md Rule 14).
  Auto-escalate (sonnet → opus on failure) is the trigger for the
  transition.
- Disable the harness's ast_check, codec, or runner gates to make a port
  pass. Fix the port.
- Skip mutation-prove (broken < 0.55, ideally < 0.45 to clear the danger
  zone — see worklog 006 learning #6).
- Re-introduce the absolute-path import bug: orchestrator must rewrite
  `/tmp/eval_<fn>/port.ts`'s absolute imports to relative on promote.
  (Filed as `mpfr-ts-NEW1`; fix this in `--ship` first.)

## High-leverage next moves (per worklog 006)

1. **Land `--ship` path rewrite** (`mpfr-ts-NEW1`): make `_promote_port`
   rewrite absolute paths inline. Tests in `eval/driver/tests/test_ralph.py`.
   Cost: 1 hour. Removes the manual rewrite step from every batch.
2. **Mega-batch (15-20) of conversion/get/set variants**: e.g.
   `mpfr_set_q`, `mpfr_set_str`, `mpfr_get_str`, `mpfr_get_z`, `mpfr_set_si_2exp`,
   `mpfr_set_ui_2exp`, ... 85 → ~100.
3. **Mega-batch (10-15) of modular/remainder**: `mpfr_fmod`, `mpfr_modf`,
   `mpfr_remainder`, `mpfr_remquo`. 100 → ~115.
4. **Phase transition** to Production, then transcendentals.

## Pickup-on-different-device checklist

Same as predecessor:

1. `git clone git@github.com:tobiasosborne/mpfr-ts.git`
2. `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. `curl -fsSL https://bun.sh/install | bash`
4. `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. `pip install pytest` (or use whatever Python env)
6. `bd bootstrap --yes && bd hooks install && bd import`
7. `bun x tsc --noEmit && bun eval/acceptance/step5/run.ts && /home/tobias/.local/bin/pytest eval/driver/tests/`
8. Read this file. Then read `docs/worklog/006-scale-out-engine.md`.

## Open bd issues at session close

See worklog 006 §"Open bd issues at session close" for the priority-ranked
list. The one P2 worth fixing first: **`mpfr-ts-NEW1`** (--ship's
promote doesn't rewrite absolute paths — manual sed every batch is the
last orchestrator-action gap before truly atomic 3-action batches).

## Don't trust this file in isolation

This `HANDOFF.md` is a pointer; the substance lives in worklog 006.
If you find yourself acting on something here without checking the
worklog, you are skipping the contract.
