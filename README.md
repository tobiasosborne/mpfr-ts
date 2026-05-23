# mpfr-ts

Auto-ported pure-TypeScript reimplementation of [GNU MPFR](https://www.mpfr.org/), produced by a ralph loop driving sonnet L3 across the library root-to-leaves of the call graph. The repository contains both the port (`src/`) and the eval harness that produced it (`eval/`). See `CLAUDE.md` for the laws, rules, and architectural rationale.

## Current phase

`PHASE.md` records the active phase (`Pilot` | `Production` | `Optimize`) and gates which workflows are valid. The pilot scope and step list live in `docs/PILOT_PLAN.md`.

## How to run

System deps: `bun >= 1.3`, `gcc`, `sqlite3`, `libmpfr-dev`, `libgmp-dev`. MPFR upstream is cloned into `./mpfr/` (gitignored) per machine.

```bash
bun --version                # >=1.3
bun test eval/integration/   # cross-function integration suite
bun run grade --function mpfr_add --port src/internal/mpfr/add.ts \
  --golden eval/functions/mpfr_add/golden.jsonl --output /tmp/grade.json
```

The published library carries zero runtime dependencies and runs on Bun or Node ≥22; the eval harness is Bun-native.
