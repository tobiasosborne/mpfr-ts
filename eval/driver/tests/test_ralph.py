"""Tests for the scale-out engine modes of eval/driver/ralph.py.

Strict-TDD: tests for ``--next``, ``--grade``, ``--commit-batch`` are
written BEFORE the implementation lands. The previously-shipped
``--dry-run`` and ``--list-pending`` modes have regression tests at the
bottom.

Mocking strategy: subprocess calls to ``bun``, ``git``, and ``bd`` are
monkeypatched (the harness must NOT actually grade, commit, or push
during unit tests). State.db writes are scoped to per-test temp DBs
created from ``eval/driver/schema.sql`` so the canonical
``eval/state.db`` is untouched.

Run with::

    pytest eval/driver/tests/test_ralph.py -v
"""

from __future__ import annotations

import json
import os
import sqlite3
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import pytest

# Make eval/driver/ importable as a package-less module set.
DRIVER_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(DRIVER_DIR))

# These imports will fail in the RED phase — that's the point. (The
# existing `ralph` module already has --dry-run/--list-pending; the new
# helpers below appear only once the GREEN implementation lands.)
import ralph  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[3]
SCHEMA_SQL = REPO_ROOT / "eval" / "driver" / "schema.sql"


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------


@pytest.fixture
def fresh_db(tmp_path: Path) -> Path:
    """A brand-new state.db with the production schema applied."""
    db_path = tmp_path / "state.db"
    schema = SCHEMA_SQL.read_text(encoding="utf-8")
    with sqlite3.connect(db_path) as conn:
        conn.executescript(schema)
    return db_path


def _insert_function(
    db_path: Path,
    *,
    name: str,
    class_: str = "misc",
    deps: list[str] | None = None,
    status: str = "done",
    topo_rank: int = 0,
) -> None:
    """Helper to seed a function row in a test DB."""
    with sqlite3.connect(db_path) as conn:
        conn.execute(
            "INSERT INTO functions (name, class, signature, deps, status, "
            "attempts, escalated, topo_rank) "
            "VALUES (?, ?, '', ?, ?, 0, 0, ?)",
            (name, class_, json.dumps(deps or []), status, topo_rank),
        )


@pytest.fixture
def fake_callgraph(tmp_path: Path) -> Path:
    """Minimal fake callgraph.json suitable for selection tests."""
    cg = {
        "generated_at": "2026-05-24T00:00:00Z",
        "mpfr_source_root": "mpfr/src",
        "functions": {
            "mpfr_a": {
                "deps": [],
                "class": "misc",
                "topo_rank": 0,
                "defined_in": "a.c",
            },
            "mpfr_b": {
                "deps": [],
                "class": "misc",
                "topo_rank": 1,
                "defined_in": "b.c",
            },
            "mpfr_c": {
                "deps": [],
                "class": "arithmetic",
                "topo_rank": 2,
                "defined_in": "c.c",
            },
        },
    }
    p = tmp_path / "callgraph.json"
    p.write_text(json.dumps(cg, indent=2), encoding="utf-8")
    return p


# ---------------------------------------------------------------------------
# Mode 1: --next — selection algorithm
# ---------------------------------------------------------------------------


def test_next_picks_all_when_no_deps(
    tmp_path: Path, fresh_db: Path, fake_callgraph: Path
) -> None:
    """Empty state.db + 3 functions with no deps → all 3 picked."""
    candidates = ralph.compute_candidates(
        ralph.load_callgraph(fake_callgraph),
        ralph.load_state(fresh_db),
        include_pending_deps=False,
        class_filter=None,
    )
    names = [c.name for c in candidates]
    assert set(names) == {"mpfr_a", "mpfr_b", "mpfr_c"}
    # Sorted by topo_rank.
    assert names == ["mpfr_a", "mpfr_b", "mpfr_c"]


def test_next_skips_blocked_status(
    tmp_path: Path, fresh_db: Path, fake_callgraph: Path
) -> None:
    """A function with status='blocked' must NOT be picked.

    `blocked` is the deliberate state for a function whose harness
    prerequisite is unmet (e.g. mpfr_abort_prec_max awaiting
    expected_throw support). Including it in candidates would force
    the orchestrator to re-discover the block every batch. Ref:
    mpfr-ts-1jr Phase B regression.
    """
    _insert_function(fresh_db, name="mpfr_a", status="blocked")
    candidates = ralph.compute_candidates(
        ralph.load_callgraph(fake_callgraph),
        ralph.load_state(fresh_db),
        include_pending_deps=False,
        class_filter=None,
    )
    names = {c.name for c in candidates}
    assert "mpfr_a" not in names
    # Other no-status functions still pickable.
    assert {"mpfr_b", "mpfr_c"}.issubset(names)


def test_next_skips_unsatisfied_deps(
    tmp_path: Path, fresh_db: Path
) -> None:
    """fn3 has dep on fn4 (pending) — fn3 skipped; fn2 dep=[fn1 done] picked."""
    cg = {
        "functions": {
            "fn1": {"deps": [], "class": "misc", "topo_rank": 0, "defined_in": "1.c"},
            "fn2": {"deps": ["fn1"], "class": "misc", "topo_rank": 1, "defined_in": "2.c"},
            "fn3": {"deps": ["fn4"], "class": "misc", "topo_rank": 2, "defined_in": "3.c"},
            "fn4": {"deps": [], "class": "misc", "topo_rank": 3, "defined_in": "4.c"},
        }
    }
    cg_path = tmp_path / "cg.json"
    cg_path.write_text(json.dumps(cg))
    _insert_function(fresh_db, name="fn1", status="done")
    # fn4 still pending implicitly (not in state.db).
    candidates = ralph.compute_candidates(
        ralph.load_callgraph(cg_path),
        ralph.load_state(fresh_db),
        include_pending_deps=False,
        class_filter=None,
    )
    names = {c.name for c in candidates}
    assert "fn2" in names
    assert "fn3" not in names
    assert "fn1" not in names  # already done
    assert "fn4" in names  # no deps, picked


def test_next_include_pending_deps_overrides_skip(
    tmp_path: Path, fresh_db: Path
) -> None:
    """--include-pending-deps lets fn3 (dep=[fn4 pending]) through."""
    cg = {
        "functions": {
            "fn1": {"deps": [], "class": "misc", "topo_rank": 0, "defined_in": "1.c"},
            "fn2": {"deps": ["fn1"], "class": "misc", "topo_rank": 1, "defined_in": "2.c"},
            "fn3": {"deps": ["fn4"], "class": "misc", "topo_rank": 2, "defined_in": "3.c"},
            "fn4": {"deps": [], "class": "misc", "topo_rank": 3, "defined_in": "4.c"},
        }
    }
    cg_path = tmp_path / "cg.json"
    cg_path.write_text(json.dumps(cg))
    _insert_function(fresh_db, name="fn1", status="done")
    candidates = ralph.compute_candidates(
        ralph.load_callgraph(cg_path),
        ralph.load_state(fresh_db),
        include_pending_deps=True,
        class_filter=None,
    )
    names = {c.name for c in candidates}
    assert "fn2" in names
    assert "fn3" in names
    assert "fn4" in names
    assert "fn1" not in names


def test_next_class_filter(
    tmp_path: Path, fresh_db: Path, fake_callgraph: Path
) -> None:
    """class=arithmetic returns only mpfr_c."""
    candidates = ralph.compute_candidates(
        ralph.load_callgraph(fake_callgraph),
        ralph.load_state(fresh_db),
        include_pending_deps=False,
        class_filter="arithmetic",
    )
    names = [c.name for c in candidates]
    assert names == ["mpfr_c"]


def test_next_batch_size_limit(
    tmp_path: Path, fresh_db: Path
) -> None:
    """5 candidates available, --batch-size 2 → 2 selected."""
    cg = {
        "functions": {
            f"fn{i}": {
                "deps": [],
                "class": "misc",
                "topo_rank": i,
                "defined_in": f"{i}.c",
            }
            for i in range(5)
        }
    }
    cg_path = tmp_path / "cg.json"
    cg_path.write_text(json.dumps(cg))
    candidates = ralph.compute_candidates(
        ralph.load_callgraph(cg_path),
        ralph.load_state(fresh_db),
        include_pending_deps=False,
        class_filter=None,
    )
    # compute_candidates returns ALL candidates; the CLI applies the
    # batch-size limit. Verify both behaviours:
    assert len(candidates) == 5
    # Now via the high-level run_next, batch-size truncates.
    selected = ralph.select_next(
        candidates, batch_size=2
    )
    assert len(selected) == 2
    assert [c.name for c in selected] == ["fn0", "fn1"]


def test_next_seeds_state_db(
    tmp_path: Path, fresh_db: Path, fake_callgraph: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Selected functions get state.db rows with correct fields."""
    # Re-route /tmp to tmp_path to keep test side effects contained.
    monkeypatch.setattr(ralph, "TMP_ROOT", tmp_path / "tmp")
    rc = ralph.run_next(
        db_path=fresh_db,
        callgraph_path=fake_callgraph,
        batch_size=5,
        class_filter=None,
        include_pending_deps=False,
        repo_root=REPO_ROOT,
    )
    assert rc == 0
    with sqlite3.connect(fresh_db) as conn:
        rows = conn.execute(
            "SELECT name, class, deps, status, topo_rank, attempts, escalated "
            "FROM functions ORDER BY topo_rank"
        ).fetchall()
    names = [r[0] for r in rows]
    assert names == ["mpfr_a", "mpfr_b", "mpfr_c"]
    # Class is preserved from callgraph.
    assert rows[2][1] == "arithmetic"
    # Deps is JSON-encoded list.
    assert json.loads(rows[0][2]) == []
    # Status pending.
    assert all(r[3] == "pending" for r in rows)
    # Topo rank preserved.
    assert [r[4] for r in rows] == [0, 1, 2]
    # Attempts / escalated default to 0.
    assert all(r[5] == 0 for r in rows)
    assert all(r[6] == 0 for r in rows)


def test_next_creates_tmp_dirs(
    tmp_path: Path, fresh_db: Path, fake_callgraph: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """/tmp/eval_<fn>/ directories are created for each selected function."""
    tmp_root = tmp_path / "tmp"
    monkeypatch.setattr(ralph, "TMP_ROOT", tmp_root)
    ralph.run_next(
        db_path=fresh_db,
        callgraph_path=fake_callgraph,
        batch_size=5,
        class_filter=None,
        include_pending_deps=False,
        repo_root=REPO_ROOT,
    )
    assert (tmp_root / "eval_mpfr_a").is_dir()
    assert (tmp_root / "eval_mpfr_b").is_dir()
    assert (tmp_root / "eval_mpfr_c").is_dir()


def test_next_output_has_selected_lines_and_prep_separator(
    tmp_path: Path, fresh_db: Path, fake_callgraph: Path, capsys: pytest.CaptureFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Stdout has machine-parseable SELECTED lines + ---PREP-PROMPT--- separator."""
    monkeypatch.setattr(ralph, "TMP_ROOT", tmp_path / "tmp")
    ralph.run_next(
        db_path=fresh_db,
        callgraph_path=fake_callgraph,
        batch_size=5,
        class_filter=None,
        include_pending_deps=False,
        repo_root=REPO_ROOT,
    )
    captured = capsys.readouterr()
    assert "---PREP-PROMPT---" in captured.out
    pre, post = captured.out.split("---PREP-PROMPT---", 1)
    selected_lines = [
        line for line in pre.splitlines() if line.startswith("SELECTED")
    ]
    assert len(selected_lines) == 3
    # Each line: SELECTED <name> <class> rank=<int>
    for line in selected_lines:
        parts = line.split()
        assert parts[0] == "SELECTED"
        assert parts[1].startswith("mpfr_")
        assert parts[2] in {
            "arithmetic", "conversion", "transcendental", "misc", "substrate"
        }
        assert parts[3].startswith("rank=")
    # Prep prompt section mentions the function names + class.
    assert "mpfr_a" in post
    assert "mpfr_c" in post


def test_next_mpn_substrate_dep_satisfied(
    tmp_path: Path, fresh_db: Path
) -> None:
    """A candidate depending only on a done mpn_* function is picked."""
    cg = {
        "functions": {
            "mpn_add_n": {
                "deps": [],
                "class": "substrate",
                "topo_rank": 0,
                "defined_in": "add_n.c",
            },
            "mpfr_use_mpn": {
                "deps": ["mpn_add_n"],
                "class": "misc",
                "topo_rank": 5,
                "defined_in": "use.c",
            },
        }
    }
    cg_path = tmp_path / "cg.json"
    cg_path.write_text(json.dumps(cg))
    _insert_function(
        fresh_db, name="mpn_add_n", class_="substrate", status="done"
    )
    candidates = ralph.compute_candidates(
        ralph.load_callgraph(cg_path),
        ralph.load_state(fresh_db),
        include_pending_deps=False,
        class_filter=None,
    )
    names = {c.name for c in candidates}
    assert "mpfr_use_mpn" in names


# ---------------------------------------------------------------------------
# Mode 2: --grade
# ---------------------------------------------------------------------------


def _seed_pending_function(
    db_path: Path, name: str, class_: str = "misc"
) -> None:
    """Seed a pending row so --grade has something to UPDATE."""
    _insert_function(
        db_path,
        name=name,
        class_=class_,
        deps=[],
        status="pending",
        topo_rank=0,
    )


def _write_passing_grade(grade_path: Path, fn: str) -> None:
    grade_path.write_text(
        json.dumps(
            {
                "composite_correctness": 1.0,
                "n_cases": 20,
                "n_pass": 20,
                "n_throw": 0,
                "n_timegate": 0,
                "n_infloop": 0,
                "first_error": None,
                "wall_ms": 12.3,
                "function": fn,
                "class": "misc",
                "schema_violation": False,
            }
        )
    )


def _write_failing_grade(grade_path: Path, fn: str) -> None:
    grade_path.write_text(
        json.dumps(
            {
                "composite_correctness": 0.5,
                "n_cases": 20,
                "n_pass": 10,
                "n_throw": 10,
                "n_timegate": 0,
                "n_infloop": 0,
                "first_error": "case#3: expected 1 got 0",
                "wall_ms": 15.0,
                "function": fn,
                "class": "misc",
                "schema_violation": False,
            }
        )
    )


@dataclass
class _RecordedCall:
    cmd: list[str]
    cwd: Path | None


def _make_grade_subprocess_stub(
    *,
    record: list[_RecordedCall],
    grade_writer,
) -> Any:
    """Build a subprocess.run replacement that records calls and writes
    the requested grade.json instead of actually invoking bun.
    """

    def fake_run(
        cmd: list[str],
        *,
        capture_output: bool = False,
        text: bool = False,
        check: bool = False,
        cwd: Path | None = None,
        **kwargs: Any,
    ) -> subprocess.CompletedProcess:
        record.append(_RecordedCall(cmd=list(cmd), cwd=cwd))
        # Find --output flag and write a grade.json there.
        if "--output" in cmd:
            out_idx = cmd.index("--output") + 1
            out_path = Path(cmd[out_idx])
            fn_idx = cmd.index("--function") + 1
            grade_writer(out_path, cmd[fn_idx])
        return subprocess.CompletedProcess(
            args=cmd, returncode=0, stdout="", stderr=""
        )

    return fake_run


def test_grade_passing_port_marks_done(
    tmp_path: Path, fresh_db: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A grade.json with composite=1.0 → status=done, runs row, exit 0."""
    _seed_pending_function(fresh_db, "mpfr_x")
    port = tmp_path / "eval_mpfr_x" / "port.ts"
    port.parent.mkdir(parents=True)
    port.write_text("export const x = 1;")
    monkeypatch.setattr(ralph, "TMP_ROOT", tmp_path)
    # Stub libmpfr-derived golden path discovery.
    golden = REPO_ROOT / "eval" / "functions" / "mpfr_x" / "golden.jsonl"

    def fake_golden(fn: str, repo_root: Path) -> Path:
        return tmp_path / f"golden_{fn}.jsonl"

    fake_golden_path = tmp_path / "golden_mpfr_x.jsonl"
    fake_golden_path.write_text("{}")
    monkeypatch.setattr(ralph, "resolve_golden_path", fake_golden)

    calls: list[_RecordedCall] = []
    monkeypatch.setattr(
        subprocess,
        "run",
        _make_grade_subprocess_stub(
            record=calls, grade_writer=_write_passing_grade
        ),
    )
    rc = ralph.run_grade(
        ["mpfr_x"], db_path=fresh_db, repo_root=REPO_ROOT
    )
    assert rc == 0
    with sqlite3.connect(fresh_db) as conn:
        status, best_correct, attempts = conn.execute(
            "SELECT status, best_correctness, attempts FROM functions "
            "WHERE name='mpfr_x'"
        ).fetchone()
        runs_count = conn.execute(
            "SELECT COUNT(*) FROM runs WHERE fn_name='mpfr_x'"
        ).fetchone()[0]
    assert status == "done"
    assert best_correct == 1.0
    assert attempts == 1
    assert runs_count == 1


def test_grade_failing_port_stays_pending(
    tmp_path: Path, fresh_db: Path, monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    """A grade.json with composite=0.5 → status pending, runs row inserted, exit 1."""
    _seed_pending_function(fresh_db, "mpfr_y")
    port = tmp_path / "eval_mpfr_y" / "port.ts"
    port.parent.mkdir(parents=True)
    port.write_text("export const y = 1;")
    monkeypatch.setattr(ralph, "TMP_ROOT", tmp_path)
    fake_golden = tmp_path / "golden_mpfr_y.jsonl"
    fake_golden.write_text("{}")
    monkeypatch.setattr(
        ralph,
        "resolve_golden_path",
        lambda fn, root: tmp_path / f"golden_{fn}.jsonl",
    )

    calls: list[_RecordedCall] = []
    monkeypatch.setattr(
        subprocess,
        "run",
        _make_grade_subprocess_stub(
            record=calls, grade_writer=_write_failing_grade
        ),
    )
    rc = ralph.run_grade(
        ["mpfr_y"], db_path=fresh_db, repo_root=REPO_ROOT
    )
    assert rc == 1
    captured = capsys.readouterr()
    assert "FAILED" in captured.err
    assert "mpfr_y" in captured.err
    with sqlite3.connect(fresh_db) as conn:
        status, attempts = conn.execute(
            "SELECT status, attempts FROM functions WHERE name='mpfr_y'"
        ).fetchone()
        runs_count = conn.execute(
            "SELECT COUNT(*) FROM runs WHERE fn_name='mpfr_y'"
        ).fetchone()[0]
    assert status == "pending"
    assert attempts == 1
    assert runs_count == 1


def test_grade_missing_port_errors(
    tmp_path: Path, fresh_db: Path, monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    """Port file absent → exit 1 with clear error."""
    _seed_pending_function(fresh_db, "mpfr_missing")
    monkeypatch.setattr(ralph, "TMP_ROOT", tmp_path)
    monkeypatch.setattr(
        ralph,
        "resolve_golden_path",
        lambda fn, root: tmp_path / f"golden_{fn}.jsonl",
    )
    rc = ralph.run_grade(
        ["mpfr_missing"], db_path=fresh_db, repo_root=REPO_ROOT
    )
    assert rc == 1
    captured = capsys.readouterr()
    assert "no port" in captured.err.lower() or "not found" in captured.err.lower()


def test_grade_mixed_batch_inserts_all_runs(
    tmp_path: Path, fresh_db: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """2 passing + 1 failing → 3 runs rows, exit 1."""
    for fn in ("mpfr_p1", "mpfr_p2", "mpfr_fail"):
        _seed_pending_function(fresh_db, fn)
        port = tmp_path / f"eval_{fn}" / "port.ts"
        port.parent.mkdir(parents=True)
        port.write_text("export const x = 1;")
    monkeypatch.setattr(ralph, "TMP_ROOT", tmp_path)
    monkeypatch.setattr(
        ralph,
        "resolve_golden_path",
        lambda fn, root: tmp_path / f"golden_{fn}.jsonl",
    )
    for fn in ("mpfr_p1", "mpfr_p2", "mpfr_fail"):
        (tmp_path / f"golden_{fn}.jsonl").write_text("{}")

    def writer(out_path: Path, fn: str) -> None:
        if fn == "mpfr_fail":
            _write_failing_grade(out_path, fn)
        else:
            _write_passing_grade(out_path, fn)

    calls: list[_RecordedCall] = []
    monkeypatch.setattr(
        subprocess,
        "run",
        _make_grade_subprocess_stub(record=calls, grade_writer=writer),
    )
    rc = ralph.run_grade(
        ["mpfr_p1", "mpfr_p2", "mpfr_fail"],
        db_path=fresh_db,
        repo_root=REPO_ROOT,
    )
    assert rc == 1
    with sqlite3.connect(fresh_db) as conn:
        n = conn.execute("SELECT COUNT(*) FROM runs").fetchone()[0]
    assert n == 3


def test_grade_subprocess_called_with_expected_args(
    tmp_path: Path, fresh_db: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """The bun runner.ts subprocess is invoked with --function, --port, --golden, --output, --class."""
    _seed_pending_function(fresh_db, "mpfr_z")
    port = tmp_path / "eval_mpfr_z" / "port.ts"
    port.parent.mkdir(parents=True)
    port.write_text("export const z = 1;")
    monkeypatch.setattr(ralph, "TMP_ROOT", tmp_path)
    monkeypatch.setattr(
        ralph,
        "resolve_golden_path",
        lambda fn, root: tmp_path / f"golden_{fn}.jsonl",
    )
    (tmp_path / "golden_mpfr_z.jsonl").write_text("{}")

    calls: list[_RecordedCall] = []
    monkeypatch.setattr(
        subprocess,
        "run",
        _make_grade_subprocess_stub(
            record=calls, grade_writer=_write_passing_grade
        ),
    )
    ralph.run_grade(["mpfr_z"], db_path=fresh_db, repo_root=REPO_ROOT)
    assert len(calls) == 1
    cmd = calls[0].cmd
    assert "bun" in cmd[0]
    assert "--function" in cmd
    assert "mpfr_z" in cmd
    assert "--port" in cmd
    assert "--golden" in cmd
    assert "--output" in cmd
    assert "--class" in cmd


# ---------------------------------------------------------------------------
# Mode 3: --commit-batch
# ---------------------------------------------------------------------------


def _record_subprocess(
    record: list[_RecordedCall],
    *,
    exit_codes: dict[str, int] | None = None,
    stderr_map: dict[str, str] | None = None,
) -> Any:
    """subprocess.run replacement that records each call and returns
    configured exit codes per command keyword."""

    exit_codes = exit_codes or {}
    stderr_map = stderr_map or {}

    def fake_run(
        cmd: list[str],
        *,
        capture_output: bool = False,
        text: bool = False,
        check: bool = False,
        cwd: Path | None = None,
        **kwargs: Any,
    ) -> subprocess.CompletedProcess:
        record.append(_RecordedCall(cmd=list(cmd), cwd=cwd))
        # Pick exit code by first matching keyword.
        rc = 0
        stderr = ""
        joined = " ".join(cmd)
        for keyword, code in exit_codes.items():
            if keyword in joined:
                rc = code
                stderr = stderr_map.get(keyword, "")
                break
        return subprocess.CompletedProcess(
            args=cmd, returncode=rc, stdout="", stderr=stderr
        )

    return fake_run


def test_commit_batch_calls_export_then_git_then_push(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Successful commit: bd export → git add -A → git commit → git push."""
    calls: list[_RecordedCall] = []
    monkeypatch.setattr(
        subprocess,
        "run",
        _record_subprocess(calls, exit_codes={}),
    )
    rc = ralph.run_commit_batch("test msg", repo_root=tmp_path)
    assert rc == 0
    cmd_strings = [" ".join(c.cmd) for c in calls]
    # Required ordering.
    assert any("bd export" in s for s in cmd_strings)
    assert any("git add -A" in s for s in cmd_strings)
    assert any("git commit" in s for s in cmd_strings)
    assert any("git push" in s for s in cmd_strings)
    # Order: bd export before git commit before git push.
    bd_idx = next(i for i, s in enumerate(cmd_strings) if "bd export" in s)
    add_idx = next(i for i, s in enumerate(cmd_strings) if "git add" in s)
    commit_idx = next(i for i, s in enumerate(cmd_strings) if "git commit" in s)
    push_idx = next(i for i, s in enumerate(cmd_strings) if "git push" in s)
    assert bd_idx < add_idx < commit_idx < push_idx


def test_commit_batch_nothing_to_commit_no_push(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture,
) -> None:
    """git commit returning non-zero 'nothing to commit' → exit 0, no push."""
    calls: list[_RecordedCall] = []
    monkeypatch.setattr(
        subprocess,
        "run",
        _record_subprocess(
            calls,
            exit_codes={"git commit": 1},
            stderr_map={"git commit": "nothing to commit, working tree clean"},
        ),
    )
    rc = ralph.run_commit_batch("noop", repo_root=tmp_path)
    assert rc == 0
    cmd_strings = [" ".join(c.cmd) for c in calls]
    assert not any("git push" in s for s in cmd_strings)


def test_commit_batch_push_failure_exits_1(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture,
) -> None:
    """git push failing → exit 1 with the stderr shown."""
    calls: list[_RecordedCall] = []
    monkeypatch.setattr(
        subprocess,
        "run",
        _record_subprocess(
            calls,
            exit_codes={"git push": 128},
            stderr_map={"git push": "remote rejected (hook decline)"},
        ),
    )
    rc = ralph.run_commit_batch("msg", repo_root=tmp_path)
    assert rc == 1
    captured = capsys.readouterr()
    assert "remote rejected" in captured.err or "remote rejected" in captured.out


# ---------------------------------------------------------------------------
# Regression: existing modes
# ---------------------------------------------------------------------------


def test_dry_run_still_works(monkeypatch: pytest.MonkeyPatch) -> None:
    """--dry-run --function mpfr_init2 renders the prompt."""
    rc = ralph.main(["--dry-run", "--function", "mpfr_init2"])
    assert rc == 0


def test_list_pending_still_works(capsys: pytest.CaptureFixture) -> None:
    """--list-pending runs against the real DB and exits 0."""
    rc = ralph.main(["--list-pending"])
    assert rc == 0


def test_mutual_exclusion_next_and_grade(
    capsys: pytest.CaptureFixture,
) -> None:
    """--next combined with --grade is a usage error."""
    rc = ralph.main(["--next", "--grade", "mpfr_x"])
    assert rc == 2
    captured = capsys.readouterr()
    assert "mutually exclusive" in captured.err.lower() or "error" in captured.err.lower()


# ---------------------------------------------------------------------------
# Mode 4: --ship
# ---------------------------------------------------------------------------
#
# Helpers shared by --ship tests
# ---------------------------------------------------------------------------


def _setup_ship_env(
    tmp_path: Path,
    fresh_db: Path,
    monkeypatch: pytest.MonkeyPatch,
    fns: list[tuple[str, str]],  # (fn_name, class_)
    *,
    write_ports: set[str] | None = None,
    write_goldens: set[str] | None = None,
) -> tuple[Path, list[_RecordedCall]]:
    """Scaffold a hermetic --ship test environment.

    Creates per-function port.ts and golden.jsonl files under tmp_path,
    patches TMP_ROOT and resolve_golden_path, and injects a subprocess.run
    replacement that records calls.

    Returns (tmp_root, calls_list).
    """
    if write_ports is None:
        write_ports = {fn for fn, _ in fns}
    if write_goldens is None:
        write_goldens = {fn for fn, _ in fns}

    tmp_root = tmp_path / "tmp"
    tmp_root.mkdir(parents=True, exist_ok=True)
    monkeypatch.setattr(ralph, "TMP_ROOT", tmp_root)

    for fn, class_ in fns:
        _seed_pending_function(fresh_db, fn, class_)
        if fn in write_ports:
            port_dir = tmp_root / f"eval_{fn}"
            port_dir.mkdir(parents=True, exist_ok=True)
            (port_dir / "port.ts").write_text(
                f"// port for {fn}\nexport const stub = 1;\n"
            )
        if fn in write_goldens:
            (tmp_path / f"golden_{fn}.jsonl").write_text("{}")

    monkeypatch.setattr(
        ralph,
        "resolve_golden_path",
        lambda fn, root: tmp_path / f"golden_{fn}.jsonl",
    )

    calls: list[_RecordedCall] = []
    return tmp_root, calls


def _make_ship_subprocess_stub(
    *,
    record: list[_RecordedCall],
    fn_grades: dict[str, str],  # fn -> "pass" | "fail"
) -> Any:
    """subprocess.run stub that handles both bun grading and git/bd calls.

    For bun calls, writes pass/fail grade.json. For git/bd calls, returns 0.
    """

    def fake_run(
        cmd: list[str],
        *,
        capture_output: bool = False,
        text: bool = False,
        check: bool = False,
        cwd=None,
        **kwargs: Any,
    ) -> subprocess.CompletedProcess:
        record.append(_RecordedCall(cmd=list(cmd), cwd=cwd))
        joined = " ".join(str(c) for c in cmd)

        if "--output" in cmd:
            # Bun grader call.
            out_idx = cmd.index("--output") + 1
            out_path = Path(cmd[out_idx])
            fn_idx = cmd.index("--function") + 1
            fn = cmd[fn_idx]
            grade = fn_grades.get(fn, "pass")
            if grade == "pass":
                _write_passing_grade(out_path, fn)
            else:
                _write_failing_grade(out_path, fn)
        # git and bd calls return 0 by default.
        return subprocess.CompletedProcess(
            args=cmd, returncode=0, stdout="committed", stderr=""
        )

    return fake_run


# ---------------------------------------------------------------------------
# Test 1: all-pass → promotes, commits, exit 0
# ---------------------------------------------------------------------------


def test_ship_all_pass_promotes_and_commits(
    tmp_path: Path, fresh_db: Path, monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    """3 functions, all composite=1.0 → runs inserted, ports promoted, commit invoked, exit 0."""
    fns = [
        ("mpfr_alpha", "misc"),
        ("mpfr_beta", "arithmetic"),
        ("mpfr_gamma", "transcendental"),
    ]
    tmp_root, calls = _setup_ship_env(tmp_path, fresh_db, monkeypatch, fns)
    repo_root = tmp_path / "repo"
    repo_root.mkdir(parents=True, exist_ok=True)
    monkeypatch.setattr(subprocess, "run", _make_ship_subprocess_stub(
        record=calls, fn_grades={fn: "pass" for fn, _ in fns},
    ))

    rc = ralph.run_ship(
        ["mpfr_alpha", "mpfr_beta", "mpfr_gamma"],
        message="batch ship test",
        db_path=fresh_db,
        repo_root=repo_root,
    )

    assert rc == 0
    captured = capsys.readouterr()
    assert "SHIPPED" in captured.out
    assert "mpfr_alpha" in captured.out
    assert "mpfr_beta" in captured.out
    assert "mpfr_gamma" in captured.out

    # All 3 runs rows inserted.
    with sqlite3.connect(fresh_db) as conn:
        n_runs = conn.execute("SELECT COUNT(*) FROM runs").fetchone()[0]
    assert n_runs == 3

    # Destination files promoted.
    assert (repo_root / "src" / "ops" / "alpha.ts").exists()
    assert (repo_root / "src" / "ops" / "beta.ts").exists()
    assert (repo_root / "src" / "ops" / "gamma.ts").exists()

    # Commit-batch was called (git commit in the subprocess calls).
    cmd_strings = [" ".join(str(x) for x in c.cmd) for c in calls]
    assert any("git commit" in s for s in cmd_strings)
    assert any("batch ship test" in s for s in cmd_strings)


# ---------------------------------------------------------------------------
# Test 2: any-fail → runs inserted, NO promote, NO commit, exit 1
# ---------------------------------------------------------------------------


def test_ship_any_fail_no_promote_no_commit(
    tmp_path: Path, fresh_db: Path, monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    """fn2 fails composite → 3 runs inserted, no promotes, no commit, stderr FAILED."""
    fns = [
        ("mpfr_p1", "misc"),
        ("mpfr_p2", "misc"),
        ("mpfr_p3", "misc"),
    ]
    tmp_root, calls = _setup_ship_env(tmp_path, fresh_db, monkeypatch, fns)
    repo_root = tmp_path / "repo"
    repo_root.mkdir(parents=True, exist_ok=True)
    monkeypatch.setattr(subprocess, "run", _make_ship_subprocess_stub(
        record=calls,
        fn_grades={"mpfr_p1": "pass", "mpfr_p2": "fail", "mpfr_p3": "pass"},
    ))

    rc = ralph.run_ship(
        ["mpfr_p1", "mpfr_p2", "mpfr_p3"],
        message="should not commit",
        db_path=fresh_db,
        repo_root=repo_root,
    )

    assert rc == 1
    captured = capsys.readouterr()
    assert "FAILED" in captured.err
    assert "mpfr_p2" in captured.err

    # All 3 runs rows inserted (grading is unconditional).
    with sqlite3.connect(fresh_db) as conn:
        n_runs = conn.execute("SELECT COUNT(*) FROM runs").fetchone()[0]
    assert n_runs == 3

    # No destination files created.
    assert not (repo_root / "src" / "ops" / "p1.ts").exists()
    assert not (repo_root / "src" / "ops" / "p2.ts").exists()
    assert not (repo_root / "src" / "ops" / "p3.ts").exists()

    # No commit called.
    cmd_strings = [" ".join(str(x) for x in c.cmd) for c in calls]
    assert not any("git commit" in s for s in cmd_strings)


# ---------------------------------------------------------------------------
# Test 3: missing /tmp port → exit 1 immediately, no grading for missing fn
# ---------------------------------------------------------------------------


def test_ship_missing_port_exits_early(
    tmp_path: Path, fresh_db: Path, monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    """One port absent → exit 1, stderr 'no port at <path>', no commit."""
    fns = [("mpfr_present", "misc"), ("mpfr_absent", "misc")]
    # Only write the port for mpfr_present; mpfr_absent has no port.
    tmp_root, calls = _setup_ship_env(
        tmp_path, fresh_db, monkeypatch, fns,
        write_ports={"mpfr_present"},
    )
    repo_root = tmp_path / "repo"
    repo_root.mkdir(parents=True, exist_ok=True)
    monkeypatch.setattr(subprocess, "run", _make_ship_subprocess_stub(
        record=calls, fn_grades={"mpfr_present": "pass"},
    ))

    rc = ralph.run_ship(
        ["mpfr_present", "mpfr_absent"],
        message="should not commit",
        db_path=fresh_db,
        repo_root=repo_root,
    )

    assert rc == 1
    captured = capsys.readouterr()
    assert "no port at" in captured.err
    assert "mpfr_absent" in captured.err

    # No commit called.
    cmd_strings = [" ".join(str(x) for x in c.cmd) for c in calls]
    assert not any("git commit" in s for s in cmd_strings)


# ---------------------------------------------------------------------------
# Test 4: substrate destination routing — mpn_* and mpfr_* substrate
# ---------------------------------------------------------------------------


def test_ship_substrate_mpn_routing(
    tmp_path: Path, fresh_db: Path, monkeypatch: pytest.MonkeyPatch,
) -> None:
    """substrate mpn_add_n → src/internal/mpn/add_n.ts (strips mpn_ prefix)."""
    repo_root = tmp_path / "repo"
    repo_root.mkdir(parents=True, exist_ok=True)
    dst = ralph._destination_path("mpn_add_n", "substrate", repo_root)
    assert dst == repo_root / "src" / "internal" / "mpn" / "add_n.ts"


def test_ship_substrate_mpfr_routing(
    tmp_path: Path,
) -> None:
    """substrate mpfr_round_raw → src/internal/mpfr/round_raw.ts (strips mpfr_ prefix)."""
    repo_root = tmp_path / "repo"
    repo_root.mkdir(parents=True, exist_ok=True)
    dst = ralph._destination_path("mpfr_round_raw", "substrate", repo_root)
    assert dst == repo_root / "src" / "internal" / "mpfr" / "round_raw.ts"


# ---------------------------------------------------------------------------
# Test 5: public function destination routing (misc/arithmetic/transcendental)
# ---------------------------------------------------------------------------


def test_ship_public_misc_routing(
    tmp_path: Path,
) -> None:
    """misc mpfr_foo → src/ops/foo.ts (strips mpfr_ prefix)."""
    repo_root = tmp_path / "repo"
    repo_root.mkdir(parents=True, exist_ok=True)
    dst = ralph._destination_path("mpfr_foo", "misc", repo_root)
    assert dst == repo_root / "src" / "ops" / "foo.ts"


def test_ship_public_arithmetic_routing(
    tmp_path: Path,
) -> None:
    """arithmetic mpfr_add → src/ops/add.ts."""
    repo_root = tmp_path / "repo"
    repo_root.mkdir(parents=True, exist_ok=True)
    dst = ralph._destination_path("mpfr_add", "arithmetic", repo_root)
    assert dst == repo_root / "src" / "ops" / "add.ts"


def test_ship_public_transcendental_routing(
    tmp_path: Path,
) -> None:
    """transcendental mpfr_exp → src/ops/exp.ts."""
    repo_root = tmp_path / "repo"
    repo_root.mkdir(parents=True, exist_ok=True)
    dst = ralph._destination_path("mpfr_exp", "transcendental", repo_root)
    assert dst == repo_root / "src" / "ops" / "exp.ts"


# ---------------------------------------------------------------------------
# Test 6: mutual exclusion — --ship with other modes
# ---------------------------------------------------------------------------


def test_ship_mutual_exclusion_with_grade(
    capsys: pytest.CaptureFixture,
) -> None:
    """--ship with --grade is a usage error (exit 2)."""
    rc = ralph.main(["--ship", "--message", "msg", "--grade", "mpfr_x", "mpfr_y"])
    assert rc == 2
    captured = capsys.readouterr()
    assert "error" in captured.err.lower()


def test_ship_mutual_exclusion_with_next(
    capsys: pytest.CaptureFixture,
) -> None:
    """--ship with --next is a usage error (exit 2)."""
    rc = ralph.main(["--ship", "--message", "msg", "--next", "mpfr_x"])
    assert rc == 2
    captured = capsys.readouterr()
    assert "error" in captured.err.lower()


def test_ship_mutual_exclusion_with_commit_batch(
    capsys: pytest.CaptureFixture,
) -> None:
    """--ship with --commit-batch is a usage error (exit 2)."""
    rc = ralph.main(["--ship", "--message", "msg", "--commit-batch", "other"])
    assert rc == 2
    captured = capsys.readouterr()
    assert "error" in captured.err.lower()


def test_ship_mutual_exclusion_with_dry_run(
    capsys: pytest.CaptureFixture,
) -> None:
    """--ship with --dry-run is a usage error (exit 2)."""
    rc = ralph.main(["--ship", "--message", "msg", "--dry-run", "mpfr_x"])
    assert rc == 2
    captured = capsys.readouterr()
    assert "error" in captured.err.lower()


def test_ship_mutual_exclusion_with_list_pending(
    capsys: pytest.CaptureFixture,
) -> None:
    """--ship with --list-pending is a usage error (exit 2)."""
    rc = ralph.main(["--ship", "--message", "msg", "--list-pending"])
    assert rc == 2
    captured = capsys.readouterr()
    assert "error" in captured.err.lower()


# ---------------------------------------------------------------------------
# Test 7: --ship without --message is a usage error
# ---------------------------------------------------------------------------


def test_ship_requires_message(
    capsys: pytest.CaptureFixture,
) -> None:
    """--ship without --message is a usage error (exit 2)."""
    rc = ralph.main(["--ship", "mpfr_foo"])
    assert rc == 2
    captured = capsys.readouterr()
    assert "error" in captured.err.lower() or captured.err  # argparse error


# ---------------------------------------------------------------------------
# Test 8: --ship with no function names is a usage error
# ---------------------------------------------------------------------------


def test_ship_requires_function_names(
    capsys: pytest.CaptureFixture,
) -> None:
    """--ship --message 'msg' with no function names is a usage error (exit 2)."""
    rc = ralph.main(["--ship", "--message", "some commit message"])
    assert rc == 2
    captured = capsys.readouterr()
    assert "error" in captured.err.lower() or captured.err


# ---------------------------------------------------------------------------
# Test 9: promote idempotency — existing file with identical content is no-op
# ---------------------------------------------------------------------------


def test_ship_promote_idempotent_same_content(
    tmp_path: Path, fresh_db: Path, monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    """Destination already exists with identical content → promote is a no-op, no error."""
    fns = [("mpfr_idem", "misc")]
    tmp_root, calls = _setup_ship_env(tmp_path, fresh_db, monkeypatch, fns)
    repo_root = tmp_path / "repo"
    repo_root.mkdir(parents=True, exist_ok=True)

    # Pre-create destination with same content.
    dst = repo_root / "src" / "ops" / "idem.ts"
    dst.parent.mkdir(parents=True, exist_ok=True)
    port_content = f"// port for mpfr_idem\nexport const stub = 1;\n"
    dst.write_text(port_content)

    monkeypatch.setattr(subprocess, "run", _make_ship_subprocess_stub(
        record=calls, fn_grades={"mpfr_idem": "pass"},
    ))

    rc = ralph.run_ship(
        ["mpfr_idem"],
        message="idempotent test",
        db_path=fresh_db,
        repo_root=repo_root,
    )
    assert rc == 0
    # File still there with same content.
    assert dst.read_text() == port_content


def test_ship_promote_overwrites_different_content(
    tmp_path: Path, fresh_db: Path, monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    """Destination exists with different content → overwritten cleanly."""
    fns = [("mpfr_overwrite", "misc")]
    tmp_root, calls = _setup_ship_env(tmp_path, fresh_db, monkeypatch, fns)
    repo_root = tmp_path / "repo"
    repo_root.mkdir(parents=True, exist_ok=True)

    # Pre-create destination with old content.
    dst = repo_root / "src" / "ops" / "overwrite.ts"
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text("// old content\n")

    monkeypatch.setattr(subprocess, "run", _make_ship_subprocess_stub(
        record=calls, fn_grades={"mpfr_overwrite": "pass"},
    ))

    rc = ralph.run_ship(
        ["mpfr_overwrite"],
        message="overwrite test",
        db_path=fresh_db,
        repo_root=repo_root,
    )
    assert rc == 0
    # File was overwritten with the new port content.
    new_content = dst.read_text()
    assert "old content" not in new_content
