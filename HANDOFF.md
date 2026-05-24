# Handoff — 50 ports landed; build the scale-out engine

You are picking up mpfr-ts after a session that took the port from 0
functions to **50** (state.db `done = 50`, all composite=1.0 against
libmpfr-derived goldens). The harness is solid. Your task is to build
the **scale-out engine** so that the next 50 → 100 → 200 → ~600
function ports happen with minimal orchestrator overhead.

## TL;DR — where to start

```bash
# 1. Pick up state
cat PHASE.md                                            # → Pilot
cat docs/worklog/005-scale-out-handoff.md               # the build spec for your work
bun x tsc --noEmit                                       # must be clean
bun eval/acceptance/step5/run.ts                         # 5/5 expected
sqlite3 eval/state.db "SELECT COUNT(*) FROM functions WHERE status='done'"   # → 50
sqlite3 eval/state.db "SELECT COUNT(*) FROM runs"        # → 50

# 2. Read the current handoff (this is NOT it — the live spec is in worklog 005)
$EDITOR docs/worklog/005-scale-out-handoff.md

# 3. Read CLAUDE.md, then PHASE.md, then the worklog stack (001 → 002 → 003 → 004 → 005)
```

## Where the live spec is

**`docs/worklog/005-scale-out-handoff.md`** is the build spec for the
scale-out engine. It is BOTH the worklog of the 50-port session AND
the handoff for the next agent. Treat that file as your contract.

The other worklog shards (001–004) give the cycle context:
- 001 — Step 5 harness skeleton
- 002 — Pilot Steps 6–10 for `mpn_add_n`
- 003 — Pilot complete (10/10)
- 004 — 50-port completion summary

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR (locked schema).
- Flip `PHASE.md` from `Pilot` to `Production` without writing
  `docs/worklog/NNN-phase-transition.md` first (per CLAUDE.md Rule 14).
- Enable auto-escalate (sonnet → opus on failure). The user's directive
  was halt-on-failure for the 50-function push; same policy for the
  next push.
- Disable the harness's ast_check, codec, or runner gates to make a
  port pass. Fix the port instead.
- Skip the mutation-prove step (broken ref + gap ≥ 0.45) on any new
  function.
- Re-introduce the path-resolution bug (`pathToUrl` must resolve
  against `process.cwd()`, not `import.meta.url`).

## Pickup-on-different-device checklist

Same as the predecessor handoff:

1. `git clone git@github.com:tobiasosborne/mpfr-ts.git`
2. Install system deps (Ubuntu): `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. Install Bun: `curl -fsSL https://bun.sh/install | bash`
4. Clone MPFR upstream: `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. Hydrate memory: `cp docs/memory/*.md ~/.claude/projects/<proj-key>/memory/`
6. `bd bootstrap --yes && bd hooks install`
7. `bd import` (loads `.beads/issues.jsonl` into the local Dolt DB)
8. `bd ready` and `sqlite3 eval/state.db "SELECT name FROM functions WHERE status='pending' ORDER BY topo_rank LIMIT 5"` to see what's next.
9. Read this file. Then read `docs/worklog/005-scale-out-handoff.md`.

## Open bd issues

See `docs/worklog/005-scale-out-handoff.md §"Open bd issues affecting
scale-out"` for the priority-ranked list. The one P2 worth fixing
before more public ports: **`mpfr-ts-wli`** (ast_check false-positive
on `import { type X }` mixed syntax — workaround is split imports, but
fixing it lets future ports use the cleaner form).

## Don't trust this file in isolation

This `HANDOFF.md` is a pointer; the substance lives in worklog 005.
If you find yourself acting on something here without checking the
worklog, you are skipping the contract.
