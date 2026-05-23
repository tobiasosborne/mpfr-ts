# Memory snapshot

These files are a snapshot of the persistent project memory normally
kept under `~/.claude/projects/-home-tobiasosborne-Projects-mpfr-ts/memory/`
on the originating machine. They are committed here so the project can
be picked up on a different device without losing the load-bearing
design decisions.

## Cross-device setup

On a fresh machine after cloning this repo:

```bash
# Copy the snapshot into Claude Code's per-project memory dir.
PROJ_KEY=$(pwd | sed 's|/|-|g')
DEST="$HOME/.claude/projects/${PROJ_KEY}/memory"
mkdir -p "$DEST"
cp docs/memory/*.md "$DEST/"
```

(The exact path format is `~/.claude/projects/<slash-replaced-abs-path>/memory/`.
On the originating device this resolves to
`~/.claude/projects/-home-tobiasosborne-Projects-mpfr-ts/memory/`.)

After that, future Claude Code sessions in this repo will pick up the
memory automatically.

## Drift policy

The in-repo `docs/memory/` snapshot is authoritative for cross-device
pickup; the live `~/.claude/projects/.../memory/` dir is authoritative
for ongoing sessions. They drift unless re-snapshotted. At session
close, refresh the snapshot if any memory file changed:

```bash
cp ~/.claude/projects/-home-tobiasosborne-Projects-mpfr-ts/memory/*.md docs/memory/
git add docs/memory/ && git commit -m "memory: snapshot updates"
```

A `scripts/sync-memory.sh` could automate this; deferred.

## What's in here

See `MEMORY.md` (the index) for the canonical list. Briefly:

- **user_profile.md** — context on the user (engineering background, communication style).
- **project_mpfr_goal.md** — the northstar.
- **decision_*.md** — the locked architectural choices (API shape, substrate strategy, failure policy, perf gate, library coherence, runtime).
- **feedback_quality_bar.md** — "uncompromising senior TS expert" is the resolver for ambiguity; spawn review subagents after substantial work.
- **mpfr_storage_traps.md** — three load-bearing facts about MPFR's value model that bit Step 4 and will bite future authors.
- **reference_auto_port_eval.md** — pointer to the predecessor project.
