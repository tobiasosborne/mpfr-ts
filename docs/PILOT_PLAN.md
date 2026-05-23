# Pilot — Vertical Slice for `mpn_add_n`

The Pilot's deliverable is **the harness, proven sound**, not the ports. We
build everything needed to grade *one* function (`mpn_add_n`) end-to-end,
then drive sonnet L3 against it once. Success means: the reference port
scores ≥0.99, a deliberately-broken port scores ≤0.5, and the live sonnet
L3 attempt produces a port that scores ≥0.95 *and* imports the locked
`src/core.ts` schema (Law 4).

Everything is **strictly serial**. One subagent at a time. We collect
learnings between steps. Premature parallelism would hide which step
introduced a regression.

## The 10 steps

| # | Step | Deliverables | Acceptance |
|---|---|---|---|
| 1 | Repo skeleton + system deps | `package.json`, `bunfig.toml`, `tsconfig.json`, `.gitignore`, `eval/`/`src/`/`docs/` dirs, MPFR clone at `./mpfr/`, verified deps | `bun --version`, `gcc --version`, `sqlite3 --version`, `pkg-config --cflags mpfr` all succeed; `ls mpfr/src/add.c` works |
| 2 | Locked schema | `src/core.ts` exactly as specified in CLAUDE.md §"Library coherence" | `bun -e "import('./src/core.ts').then(m => console.log(Object.keys(m)))"` lists MPFRError + types |
| 3 | State DB | `eval/driver/schema.sql`, `eval/state.db` created and populated with `mpn_add_n` row | `sqlite3 eval/state.db "SELECT name, status FROM functions"` shows `mpn_add_n\|pending` |
| 4 | Golden-driver common.h | `eval/golden_master/common.h` with xorshift, JSONL helpers, GMP-limb-array serializer, MPFR-value serializer | A 5-line test driver compiles + emits one valid JSON line |
| 5 | Harness skeleton | `eval/harness/runner.ts`, `worker.ts`, `value_codec.ts`, `ast_check.ts` | Smoke test: runs against a stub port + stub golden, produces grade.json |
| 6 | `mpn_add_n` spec + golden | `eval/functions/mpn_add_n/spec.json`, `golden_driver.c`, `golden.jsonl` | `wc -l golden.jsonl ≥ 150`; happy/edge/adversarial/fuzz all present |
| 7 | Reference ports | `eval/reference_ports/correct/mpn_add_n.ts`, `eval/reference_ports/broken/mpn_add_n.ts` | Both files import from `src/core.ts` (n/a for mpn — see step 7 spec), match the spec signature |
| 8 | Mutation-prove the golden | Run runner against both reference ports | Correct → composite ≥ 0.99; broken → composite ≤ 0.5; gap ≥ 0.49 |
| 9 | Prompt template + dry-run driver | `eval/driver/prompts.py`, `eval/driver/ralph.py --dry-run` | Dry-run prints a complete prompt including `src/core.ts`, C source, spec, iteration loop instructions |
| 10 | Live sonnet L3 attempt | One Agent call against `mpn_add_n`, port written to `/tmp/`, graded, recorded in state.db | composite ≥ 0.95 AND port imports core types AND state.db row updated |

## Halt-on-failure (Pilot Rule PIL.1)

Any step that fails its acceptance criterion **stops the pilot**. The
fix is either: (a) the spec is wrong, (b) the harness is wrong, or
(c) the prompt is wrong. Once fixed, restart the affected step and
verify all *downstream* steps still pass (CLAUDE.md PIL.4).

## Beads correspondence

Each step has a bd issue. Status flows `open → in_progress → closed`.
Issues that surface mid-step are filed separately as `bug` and linked
via `bd dep add`. Run `bd ready` at any point to see what's
unblocked.

## What this does NOT cover

- Steps 11+ (the other 9 pilot functions, then Production) — these
  start only after Step 10 is green and a worklog shard captures the
  Pilot retrospective.
- Cross-function integration tests (Law 4) — those need ≥2 ported
  public functions to exercise; deferred to the pilot's second
  function (`mpfr_init2` + `mpfr_set_d`).
- Auto-escalation to opus — disabled in Pilot per PIL.4.
