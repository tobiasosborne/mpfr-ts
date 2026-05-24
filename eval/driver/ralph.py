#!/usr/bin/env python3
"""Ralph-loop driver CLI for mpfr-ts (scale-out engine).

Originally the Pilot Step 9 dry-run driver; extended in worklog 005 with
three batched modes that compress the orchestrator's per-batch command
count from ~7-10 to ~3.

CLI surface
-----------
::

    python3 eval/driver/ralph.py --dry-run --function FN
        # Render the full prompt for FN. (Pilot Step 9.)

    python3 eval/driver/ralph.py --list-pending
        # Print pending-status functions, sorted by topo_rank.

    python3 eval/driver/ralph.py --next [--batch-size N] \\
        [--filter class=X] [--include-pending-deps]
        # Pick next N pending functions, seed state.db, print a
        # SELECTED manifest plus a prep-subagent prompt skeleton.

    python3 eval/driver/ralph.py --grade <fn1> <fn2> ...
        # Spot-grade each /tmp/eval_<fn>/port.ts, INSERT runs row,
        # UPDATE functions row. Exit 0 iff every fn passes composite>=0.95.

    python3 eval/driver/ralph.py --commit-batch <msg>
        # bd export, git add -A, git commit, git push. Exit 0 on success;
        # exit 0 with no-op message if there's nothing to commit.

Modes are mutually exclusive. Selecting more than one or none is a
usage error.

References
----------
- CLAUDE.md, especially Rule 9 (state.db is the only persistent tracker)
  and Rule 11 (no remote CI; quality gates run locally).
- docs/worklog/005-scale-out-handoff.md §"Suggested ralph.py CLI surface".
- bd issue ``mpfr-ts-29c`` (step 5 of the scale-out engine).
"""

from __future__ import annotations

import argparse
import json
import os
import sqlite3
import subprocess
import sys
import time
import uuid
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

from prompts import build_prompt


# ---------------------------------------------------------------------------
# Module-level constants
# ---------------------------------------------------------------------------


#: Root directory under which per-function eval scratch dirs live. Tests
#: monkeypatch this so they don't pollute /tmp.
TMP_ROOT: Path = Path("/tmp")


#: Threshold for composite_correctness above which a port is considered
#: ``done``. Matches the threshold used by the prompt template and
#: ``mutation_prove.py``.
PASS_THRESHOLD: float = 0.95


#: Valid runner classes. ``conversion`` is mapped to ``misc`` because
#: runner.ts rejects ``--class conversion`` (see worklog 005 hard-won
#: lesson #1).
_RUNNER_CLASSES: frozenset[str] = frozenset({
    "substrate", "arithmetic", "transcendental", "misc",
})


# ---------------------------------------------------------------------------
# Repo root helper
# ---------------------------------------------------------------------------


def _repo_root() -> Path:
    """Repo root inferred from this file's location.

    ``eval/driver/ralph.py`` lives two directories below the repo root.
    Anchoring to ``__file__`` keeps the script working regardless of cwd.
    """
    return Path(__file__).resolve().parents[2]


# ---------------------------------------------------------------------------
# Data types
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class CandidateFn:
    """A pending function eligible for the next batch."""

    name: str
    class_: str
    topo_rank: int
    deps: tuple[str, ...]
    defined_in: str


# ---------------------------------------------------------------------------
# Pure: callgraph + state-db loaders (testable in isolation)
# ---------------------------------------------------------------------------


def load_callgraph(path: Path) -> dict[str, dict]:
    """Read the JSON callgraph manifest.

    Returns the ``functions`` dict (keyed by function name). Raises
    ``FileNotFoundError`` if absent; ``ValueError`` if malformed.
    """
    text = path.read_text(encoding="utf-8")
    payload = json.loads(text)
    fns = payload.get("functions")
    if not isinstance(fns, dict):
        raise ValueError(
            f"{path}: expected top-level 'functions' dict, got {type(fns).__name__}"
        )
    return fns


def load_state(db_path: Path) -> dict[str, str]:
    """Return a mapping ``name -> status`` from ``state.db``.

    Opens read-only via the ``file:...?mode=ro`` URI form. Functions
    absent from state.db are implicitly pending (caller's responsibility
    to treat them so).
    """
    if not db_path.exists():
        # Treat a missing DB as "no rows yet"; callers can still proceed.
        return {}
    uri = f"file:{db_path}?mode=ro"
    with sqlite3.connect(uri, uri=True) as conn:
        rows = conn.execute("SELECT name, status FROM functions").fetchall()
    return {name: status for name, status in rows}


def _is_dep_satisfied(dep: str, state: dict[str, str]) -> bool:
    """A dep is satisfied iff it has status='done' in state.db.

    Per worklog 005 (mpn_* substrate clarification, bd ``mpfr-ts-9li``):
    mpn_* substrate counts the same way — the dep must be marked done.
    There is no implicit-success for mpn_* deps that aren't yet ported.
    """
    return state.get(dep) == "done"


def compute_candidates(
    callgraph: dict[str, dict],
    state: dict[str, str],
    *,
    include_pending_deps: bool,
    class_filter: str | None,
) -> list[CandidateFn]:
    """Return every callgraph function that is eligible for porting.

    A function is eligible iff:

    1. Either absent from state.db OR has status ``pending``. Status
       ``done`` / ``slow`` / ``parked`` / ``blocked`` / ``in_flight``
       all skip — ``blocked`` is deliberately not pickable until
       manually un-blocked (e.g. a harness dependency is resolved);
       ``in_flight`` means another worker is already on it; ``done``
       and the rest are self-explanatory.
    2. Every dep is either marked ``done`` in state.db OR
       ``--include-pending-deps`` is set.
    3. If ``class_filter`` is set, the function's class matches.

    Returns candidates sorted by ``(topo_rank, name)`` ascending.
    """

    eligible_status = {"pending"}
    candidates: list[CandidateFn] = []
    for name, entry in callgraph.items():
        status = state.get(name)
        if status is not None and status not in eligible_status:
            continue
        deps = entry.get("deps", [])
        if not include_pending_deps:
            if not all(_is_dep_satisfied(d, state) for d in deps):
                continue
        cls = entry.get("class", "misc")
        if class_filter is not None and cls != class_filter:
            continue
        candidates.append(
            CandidateFn(
                name=name,
                class_=cls,
                topo_rank=int(entry.get("topo_rank", 0)),
                deps=tuple(deps),
                defined_in=entry.get("defined_in", ""),
            )
        )
    candidates.sort(key=lambda c: (c.topo_rank, c.name))
    return candidates


def select_next(
    candidates: Iterable[CandidateFn], *, batch_size: int
) -> list[CandidateFn]:
    """Truncate ``candidates`` to ``batch_size`` entries."""
    if batch_size <= 0:
        raise ValueError(f"batch_size must be >= 1, got {batch_size}")
    return list(candidates)[:batch_size]


def seed_row(conn: sqlite3.Connection, cand: CandidateFn) -> None:
    """INSERT a pending row for ``cand`` if absent. Idempotent."""
    conn.execute(
        "INSERT OR IGNORE INTO functions "
        "(name, class, signature, deps, status, attempts, escalated, topo_rank) "
        "VALUES (?, ?, '', ?, 'pending', 0, 0, ?)",
        (
            cand.name,
            cand.class_,
            json.dumps(list(cand.deps)),
            cand.topo_rank,
        ),
    )


# ---------------------------------------------------------------------------
# Prep-prompt skeleton
# ---------------------------------------------------------------------------


def _render_prep_prompt(
    selected: list[CandidateFn], *, repo_root: Path
) -> str:
    """Render the prep-subagent prompt skeleton.

    The orchestrator pastes this verbatim into an ``Agent`` call (after
    any amendments). Targets a single batch of related functions.
    """

    lines: list[str] = []
    lines.append(
        "You are a PREP subagent. Your job is to set up the per-function "
        "scaffolding for the following batch of mpfr-ts ports."
    )
    lines.append("")
    lines.append("Functions in this batch:")
    lines.append("")
    for cand in selected:
        c_src = f"mpfr/src/{cand.defined_in}" if cand.defined_in else "mpfr/src/<unknown>.c"
        lines.append(f"  - {cand.name}  (class={cand.class_}, source={c_src})")
    lines.append("")
    lines.append("Per-function deliverables (relative to repo root):")
    lines.append("")
    for cand in selected:
        lines.append(f"  {cand.name}:")
        lines.append(f"    - eval/functions/{cand.name}/spec.json")
        lines.append(f"    - eval/functions/{cand.name}/golden_driver.c")
        lines.append(f"    - eval/functions/{cand.name}/mined_tests.jsonl  (optional)")
        lines.append(f"    - eval/reference_ports/correct/{cand.name}.ts")
        lines.append(f"    - eval/reference_ports/broken/{cand.name}.ts")
    lines.append("")
    lines.append("Workflow (the orchestrator runs the build/run scripts AFTER you finish):")
    lines.append("")
    lines.append("  1. Read the C source for each function listed above.")
    lines.append("  2. Write spec.json + golden_driver.c per function.")
    lines.append("     spec.json must declare prec_unit, signature, classes, expected n_cases,")
    lines.append("     and a 'class' field in {substrate, arithmetic, transcendental, misc}")
    lines.append("     (use 'misc' for conversion-style ops; runner.ts rejects 'conversion').")
    lines.append("  3. Implement the correct reference port at")
    lines.append("     eval/reference_ports/correct/<fn>.ts (re-export of src/ops/<fn>.ts is OK).")
    lines.append("  4. Implement a deliberately-broken variant at")
    lines.append("     eval/reference_ports/broken/<fn>.ts (perturb one branch).")
    lines.append("  5. The orchestrator will then run:")
    lines.append("       bash eval/golden_master/build.sh")
    lines.append("       bash eval/golden_master/run_all.sh")
    lines.append("     to compile the driver and materialise the golden.jsonl files.")
    lines.append("")
    lines.append("Mutation-prove invariants (CLAUDE.md PIL.3):")
    lines.append("")
    lines.append("  - Correct port must score >= 0.95 against the generated golden.")
    lines.append("  - Broken port must score < 0.55.")
    lines.append("  - If the broken port scores between 0.45 and 0.55 (calibration danger zone),")
    lines.append("    strengthen the broken perturbation or inflate the adversarial cases until")
    lines.append("    the gap is clean.")
    lines.append("")
    lines.append("Hard constraints:")
    lines.append("")
    lines.append("  - Each spec.json's golden must satisfy minimum tag counts:")
    lines.append("    happy>=20, edge>=30, adversarial>=10, fuzz>=50, mined>=5 (or all available).")
    lines.append("  - Cap PREC_MAX cases at 4096 bits (libmpfr eats ~256 MB at PREC_MAX).")
    lines.append("  - Do NOT modify src/core.ts, eval/harness/, eval/golden_master/common.h,")
    lines.append("    or any already-ported function under src/ops/.")
    lines.append("")
    lines.append("When done, list the files you created and any calibration notes for the orchestrator.")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Mode 1: --next
# ---------------------------------------------------------------------------


def run_next(
    *,
    db_path: Path,
    callgraph_path: Path,
    batch_size: int,
    class_filter: str | None,
    include_pending_deps: bool,
    repo_root: Path,
) -> int:
    """Execute the ``--next`` mode end to end.

    Returns 0 on success, 2 on configuration error.
    """

    if not callgraph_path.exists():
        print(
            f"error: callgraph not found at {callgraph_path}; run callgraph.py first",
            file=sys.stderr,
        )
        return 2
    if not db_path.exists():
        print(f"error: state.db not found at {db_path}", file=sys.stderr)
        return 2

    callgraph = load_callgraph(callgraph_path)
    state = load_state(db_path)
    candidates = compute_candidates(
        callgraph,
        state,
        include_pending_deps=include_pending_deps,
        class_filter=class_filter,
    )
    selected = select_next(candidates, batch_size=batch_size)

    if not selected:
        print("# no candidates matched the filter", file=sys.stderr)
        return 0

    # Seed state.db rows (rw open).
    uri = f"file:{db_path}?mode=rw"
    with sqlite3.connect(uri, uri=True) as conn:
        conn.execute("PRAGMA foreign_keys = ON")
        for cand in selected:
            seed_row(conn, cand)
        conn.commit()

    # Create per-function scratch directories.
    for cand in selected:
        tmp_dir = TMP_ROOT / f"eval_{cand.name}"
        tmp_dir.mkdir(parents=True, exist_ok=True)

    # Emit the SELECTED manifest.
    for cand in selected:
        print(f"SELECTED  {cand.name}  {cand.class_}  rank={cand.topo_rank}")
    print("---PREP-PROMPT---")
    print(_render_prep_prompt(selected, repo_root=repo_root))
    return 0


# ---------------------------------------------------------------------------
# Mode 2: --grade
# ---------------------------------------------------------------------------


def resolve_port_path(fn: str, repo_root: Path) -> Path | None:
    """Find the port .ts file to grade for ``fn``.

    Priority:
      1. ``/tmp/eval_<fn>/port.ts`` (the canonical sonnet output).
      2. ``src/ops/<short_name>.ts`` where ``short_name`` strips the
         ``mpfr_`` prefix. (Used by the orchestrator for spot-grading
         an already-promoted port.)
      3. ``src/internal/mpn/<short_name>.ts`` for substrate functions.
    """

    candidate = TMP_ROOT / f"eval_{fn}" / "port.ts"
    if candidate.exists():
        return candidate

    if fn.startswith("mpfr_"):
        short = fn[len("mpfr_") :]
        canonical = repo_root / "src" / "ops" / f"{short}.ts"
        if canonical.exists():
            return canonical

    if fn.startswith("mpn_"):
        short = fn[len("mpn_") :]
        canonical = repo_root / "src" / "internal" / "mpn" / f"{short}.ts"
        if canonical.exists():
            return canonical

    return None


def resolve_golden_path(fn: str, repo_root: Path) -> Path:
    """Where the golden.jsonl for ``fn`` should live."""
    return repo_root / "eval" / "functions" / fn / "golden.jsonl"


def _runner_class(class_: str) -> str:
    """Map a state.db class to a runner-accepted class."""
    if class_ == "conversion":
        return "misc"
    if class_ in _RUNNER_CLASSES:
        return class_
    return "misc"


def _new_run_id(fn: str) -> str:
    iso = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    short = uuid.uuid4().hex[:8]
    return f"r{iso}-{short}-{fn}"


def _load_function_row(conn: sqlite3.Connection, fn: str) -> tuple[str, int] | None:
    """Return ``(class, attempts)`` for ``fn``, or None if absent."""
    row = conn.execute(
        "SELECT class, attempts FROM functions WHERE name = ?", (fn,)
    ).fetchone()
    return row


def _grade_one(
    fn: str,
    *,
    db_path: Path,
    repo_root: Path,
) -> tuple[bool, str | None, dict[str, float | int | str | None]]:
    """Grade a single function.

    Returns ``(passed, first_error, summary)``. ``summary`` carries
    composite, n_pass/n_cases, wall_ms for the stdout one-liner.
    """

    port_path = resolve_port_path(fn, repo_root)
    if port_path is None:
        return False, f"no port at /tmp/eval_{fn}/port.ts or canonical path", {
            "composite": 0.0,
            "n_pass": 0,
            "n_cases": 0,
            "wall_ms": 0.0,
            "first_error": "no port file",
        }
    golden_path = resolve_golden_path(fn, repo_root)
    if not golden_path.exists():
        return False, f"no golden at {golden_path}", {
            "composite": 0.0,
            "n_pass": 0,
            "n_cases": 0,
            "wall_ms": 0.0,
            "first_error": "no golden",
        }

    # Look up the class from state.db; default to misc.
    uri_ro = f"file:{db_path}?mode=ro"
    with sqlite3.connect(uri_ro, uri=True) as conn:
        row = _load_function_row(conn, fn)
    class_ = row[0] if row else "misc"
    runner_cls = _runner_class(class_)

    run_id = _new_run_id(fn)
    grade_path = Path(f"/tmp/grade_{run_id}.json")
    log_path = Path(f"/tmp/grade_{run_id}.log")

    cmd = [
        "bun",
        str(repo_root / "eval" / "harness" / "runner.ts"),
        "--function", fn,
        "--port", str(port_path),
        "--golden", str(golden_path),
        "--output", str(grade_path),
        "--class", runner_cls,
    ]
    started = time.time()
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=False,
            cwd=str(repo_root),
        )
    except FileNotFoundError as exc:  # pragma: no cover — missing bun
        return False, f"bun not found: {exc}", {
            "composite": 0.0,
            "n_pass": 0,
            "n_cases": 0,
            "wall_ms": 0.0,
            "first_error": "bun missing",
        }
    ended = time.time()

    # Persist the log even on failure (best-effort).
    try:
        log_path.write_text(
            f"STDOUT:\n{result.stdout}\n\nSTDERR:\n{result.stderr}",
            encoding="utf-8",
        )
    except OSError:  # pragma: no cover
        pass

    if not grade_path.exists():
        return False, f"runner did not produce {grade_path}: rc={result.returncode}", {
            "composite": 0.0,
            "n_pass": 0,
            "n_cases": 0,
            "wall_ms": 0.0,
            "first_error": result.stderr[:200] if result.stderr else "no grade.json",
        }

    grade = json.loads(grade_path.read_text(encoding="utf-8"))
    composite = float(grade.get("composite_correctness", 0.0))
    perf_grade = float(grade.get("perf_grade", 0.0))
    n_cases = int(grade.get("n_cases", 0))
    n_pass = int(grade.get("n_pass", 0))
    n_throw = int(grade.get("n_throw", 0))
    n_timegate = int(grade.get("n_timegate", 0))
    n_infloop = int(grade.get("n_infloop", 0))
    first_error = grade.get("first_error")
    wall_ms = float(grade.get("wall_ms", 0.0))

    # Persist run + update functions row.
    uri_rw = f"file:{db_path}?mode=rw"
    passed = composite >= PASS_THRESHOLD
    with sqlite3.connect(uri_rw, uri=True) as conn:
        conn.execute("PRAGMA foreign_keys = ON")
        # Ensure a functions row exists so the FK on runs.fn_name resolves.
        row = _load_function_row(conn, fn)
        if row is None:
            conn.execute(
                "INSERT INTO functions "
                "(name, class, signature, deps, status, attempts, escalated, topo_rank) "
                "VALUES (?, ?, '', '[]', 'pending', 0, 0, 0)",
                (fn, class_),
            )
        conn.execute(
            "INSERT INTO runs (run_id, fn_name, model, effort, seed, started_at, ended_at, "
            "composite_correctness, perf_grade, n_cases, n_pass, n_throw, n_timegate, n_infloop, "
            "first_error, raw_path, port_path, grade_path, usd_est) "
            "VALUES (?, ?, 'sonnet', 'L3', 0, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NULL, ?, ?, 0.0)",
            (
                run_id,
                fn,
                started,
                ended,
                composite,
                perf_grade,
                n_cases,
                n_pass,
                n_throw,
                n_timegate,
                n_infloop,
                first_error,
                str(port_path),
                str(grade_path),
            ),
        )
        if passed:
            conn.execute(
                "UPDATE functions SET attempts = attempts + 1, status = 'done', "
                "best_run_id = ?, best_correctness = ?, best_perf_grade = ? "
                "WHERE name = ?",
                (run_id, composite, perf_grade, fn),
            )
        else:
            # Halt-on-failure during Pilot: do NOT auto-park; leave pending
            # and let the orchestrator decide. Just bump attempts.
            conn.execute(
                "UPDATE functions SET attempts = attempts + 1 WHERE name = ?",
                (fn,),
            )
        conn.commit()

    summary = {
        "composite": composite,
        "n_pass": n_pass,
        "n_cases": n_cases,
        "wall_ms": wall_ms,
        "first_error": first_error,
    }
    return passed, first_error, summary


def run_grade(
    fns: list[str], *, db_path: Path, repo_root: Path
) -> int:
    """Grade each function in ``fns``. Exit 0 iff all pass."""

    if not fns:
        print("error: --grade requires at least one function name", file=sys.stderr)
        return 2

    all_passed = True
    failures: list[tuple[str, str]] = []
    for fn in fns:
        passed, first_error, summary = _grade_one(
            fn, db_path=db_path, repo_root=repo_root
        )
        status_word = "done" if passed else "FAILED"
        line = (
            f"GRADED  {fn}  "
            f"composite={summary['composite']:.4f}  "
            f"pass={summary['n_pass']}/{summary['n_cases']}  "
            f"wall={summary['wall_ms']:.0f}ms  "
            f"status={status_word}"
        )
        if not passed and summary["first_error"]:
            line += f"  first_error={summary['first_error']}"
        print(line)
        if not passed:
            all_passed = False
            failures.append((fn, str(summary.get("first_error", ""))))

    if not all_passed:
        print("", file=sys.stderr)
        for fn, err in failures:
            print(f"FAILED: {fn}  first_error={err}", file=sys.stderr)
        return 1
    return 0


# ---------------------------------------------------------------------------
# Mode 3: --commit-batch
# ---------------------------------------------------------------------------


def _run_cmd(
    cmd: list[str], *, cwd: Path
) -> subprocess.CompletedProcess:
    """Thin wrapper that always returns a CompletedProcess."""
    return subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        check=False,
        cwd=str(cwd),
    )


def run_commit_batch(message: str, *, repo_root: Path) -> int:
    """bd export, git add -A, git commit -m, git push."""

    # 1. bd export.
    bd = _run_cmd(
        ["bd", "export", "-o", ".beads/issues.jsonl"], cwd=repo_root
    )
    if bd.returncode != 0:
        # Tolerate "no changes" variants — but propagate everything else.
        msg = (bd.stderr or "") + (bd.stdout or "")
        if "no changes" not in msg.lower() and "nothing to export" not in msg.lower():
            print(f"bd export failed (rc={bd.returncode}):", file=sys.stderr)
            print(msg, file=sys.stderr)
            return 1

    # 2. git add -A.
    add = _run_cmd(["git", "add", "-A"], cwd=repo_root)
    if add.returncode != 0:
        print(f"git add -A failed (rc={add.returncode}):", file=sys.stderr)
        print(add.stderr, file=sys.stderr)
        return 1

    # 3. git commit.
    commit = _run_cmd(["git", "commit", "-m", message], cwd=repo_root)
    if commit.returncode != 0:
        combined = (commit.stdout or "") + (commit.stderr or "")
        if (
            "nothing to commit" in combined
            or "no changes added to commit" in combined
            or "working tree clean" in combined
        ):
            print("COMMIT-BATCH: nothing to commit (no-op)")
            return 0
        print(f"git commit failed (rc={commit.returncode}):", file=sys.stderr)
        print(combined, file=sys.stderr)
        return 1

    # 4. git push (no --no-verify, no --force).
    push = _run_cmd(["git", "push"], cwd=repo_root)
    if push.returncode != 0:
        print(f"git push failed (rc={push.returncode}):", file=sys.stderr)
        print(push.stderr or push.stdout, file=sys.stderr)
        return 1

    print("COMMIT-BATCH: ok")
    print("PUSH: ok")
    return 0


# ---------------------------------------------------------------------------
# Legacy modes (unchanged)
# ---------------------------------------------------------------------------


def _list_pending(db_path: Path) -> int:
    """Print pending functions from ``state.db``, lowest ``topo_rank`` first."""
    if not db_path.exists():
        print(f"error: state.db not found at {db_path}", file=sys.stderr)
        return 2
    uri = f"file:{db_path}?mode=ro"
    with sqlite3.connect(uri, uri=True) as conn:
        cur = conn.execute(
            "SELECT name FROM functions WHERE status = 'pending' "
            "ORDER BY topo_rank, name"
        )
        rows = cur.fetchall()
    for (name,) in rows:
        print(name)
    return 0


def _dry_run(function_name: str, repo_root: Path) -> int:
    """Render the prompt for ``function_name`` to stdout."""
    try:
        prompt = build_prompt(function_name, repo_root=repo_root)
    except FileNotFoundError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    except ValueError as exc:
        print(f"error: malformed spec for {function_name}: {exc}", file=sys.stderr)
        return 2
    print(prompt, end="")
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ralph.py",
        description=(
            "mpfr-ts ralph loop driver. Modes are mutually exclusive: "
            "--dry-run, --list-pending, --next, --grade, --commit-batch."
        ),
    )
    parser.add_argument(
        "--function",
        metavar="FN",
        help="C function name (required with --dry-run).",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Render the full prompt for --function to stdout and exit.",
    )
    parser.add_argument(
        "--list-pending",
        action="store_true",
        help="Print pending functions, one per line, sorted by topo_rank.",
    )
    parser.add_argument(
        "--next",
        action="store_true",
        help="Select the next batch of pending functions.",
    )
    parser.add_argument(
        "--batch-size",
        type=int,
        default=5,
        help="Batch size for --next (default: 5).",
    )
    parser.add_argument(
        "--filter",
        metavar="EXPR",
        help="Restrict --next candidates by class=X.",
    )
    parser.add_argument(
        "--include-pending-deps",
        action="store_true",
        help="Allow --next to pick functions with unsatisfied deps.",
    )
    parser.add_argument(
        "--grade",
        nargs="+",
        metavar="FN",
        help="Spot-grade each /tmp/eval_<FN>/port.ts.",
    )
    parser.add_argument(
        "--commit-batch",
        metavar="MSG",
        help="bd export + git add -A + git commit -m MSG + git push.",
    )
    return parser


def _parse_filter(expr: str | None) -> str | None:
    """Parse ``--filter class=X``. Currently only ``class=`` is supported."""
    if expr is None:
        return None
    if not expr.startswith("class="):
        raise ValueError(f"--filter must be of the form class=X, got {expr!r}")
    return expr[len("class=") :]


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)

    # Mode selection — exactly one must be set.
    modes_set = [
        bool(args.dry_run),
        bool(args.list_pending),
        bool(args.next),
        bool(args.grade),
        bool(args.commit_batch),
    ]
    if sum(modes_set) > 1:
        print(
            "error: modes (--dry-run, --list-pending, --next, --grade, "
            "--commit-batch) are mutually exclusive",
            file=sys.stderr,
        )
        return 2
    if sum(modes_set) == 0:
        print(
            "error: specify one of --dry-run, --list-pending, --next, "
            "--grade, --commit-batch",
            file=sys.stderr,
        )
        return 2

    root = _repo_root()
    db_path = root / "eval" / "state.db"

    if args.list_pending:
        return _list_pending(db_path)
    if args.dry_run:
        if not args.function:
            print("error: specify --function for dry-run mode", file=sys.stderr)
            return 2
        return _dry_run(args.function, root)
    if args.next:
        try:
            class_filter = _parse_filter(args.filter)
        except ValueError as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 2
        return run_next(
            db_path=db_path,
            callgraph_path=root / "eval" / "driver" / "callgraph.json",
            batch_size=args.batch_size,
            class_filter=class_filter,
            include_pending_deps=args.include_pending_deps,
            repo_root=root,
        )
    if args.grade:
        return run_grade(args.grade, db_path=db_path, repo_root=root)
    if args.commit_batch:
        return run_commit_batch(args.commit_batch, repo_root=root)

    return 2  # unreachable


if __name__ == "__main__":
    sys.exit(main())
