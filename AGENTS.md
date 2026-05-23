# AGENTS.md

**Read [`CLAUDE.md`](./CLAUDE.md) instead.** It is the single source of
truth for agent guidance in this repo. The Laws, Rules, phase
discipline, library-coherence schema, state-DB conventions, build
commands, and session-close protocol all live there.

This file exists only because some tooling (bd init, certain harnesses)
expects an `AGENTS.md` at the repo root. Keep this pointer up to date;
do not let the two files diverge with substantive content.

## Where things live

- Agent guidance / Laws / Rules → `CLAUDE.md`
- Current phase → `PHASE.md`
- Pilot plan → `docs/PILOT_PLAN.md`
- Issue tracking → `bd` (beads), embedded Dolt at `.beads/`
- Persistent project memory → `~/.claude/projects/-home-tobiasosborne-Projects-mpfr-ts/memory/`
- In-session task list → `TaskCreate`/`TaskUpdate` tools (ephemeral by design)

## Conflicts with bd-init defaults

The default bd `AGENTS.md` template forbids `TaskCreate` and
`MEMORY.md`. We deliberately use both, scoped per `CLAUDE.md` Rule 9:
`bd` is the only persistent cross-session tracker, but `TaskCreate` is
permitted for in-session sub-step visibility, and MEMORY.md is the
project-memory persistence mechanism (it survives sessions; bd notes
do not surface in agent context the same way).
