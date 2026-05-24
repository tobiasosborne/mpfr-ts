"""Tests for eval/driver/calibrate.py. Run: pytest eval/driver/tests/test_calibrate.py -v"""

from __future__ import annotations

import sqlite3
import sys
from pathlib import Path

DRIVER_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(DRIVER_DIR))

import calibrate  # noqa: E402
import mutate  # noqa: E402

REPO_ROOT = DRIVER_DIR.parent.parent
ADD_D_PORT = REPO_ROOT / "src" / "ops" / "add_d.ts"
ADD_D_GOLDEN = REPO_ROOT / "eval" / "functions" / "mpfr_add_d" / "golden.jsonl"


def _seed_db(path: Path, rows: list[tuple[str, str, str]]) -> None:
    """rows: (name, class, status)."""
    conn = sqlite3.connect(path)
    conn.execute("CREATE TABLE functions (name TEXT PRIMARY KEY, class TEXT, status TEXT)")
    conn.executemany("INSERT INTO functions VALUES (?, ?, ?)", rows)
    conn.commit()
    conn.close()


def _mo(name: str, comp: float | None, below: bool = False, clean: bool = False):
    return mutate.MutationOutcome(name=name, composite=comp, below_threshold=below,
                                  clean_kill=clean, module_init_failed=False)


def test_format_summary_with_mock_results() -> None:
    fix_a = calibrate.CalibrationFixture("mpfr_add_d", ADD_D_PORT, ADD_D_GOLDEN, "arithmetic")
    fix_b = calibrate.CalibrationFixture("mpfr_foo", Path("/tmp/foo.ts"), Path("/tmp/g.jsonl"), "misc")
    res_a = mutate.ProveResult("mpfr_add_d",
        [_mo("op-swap", 0.0, True, True), _mo("rnd-swap", 1.0), _mo("comparison-swap", 0.97)],
        gate_passed=True, clean_kills=1)
    res_b = mutate.ProveResult("mpfr_foo",
        [_mo("op-swap", 0.5, True, True), _mo("rnd-swap", None)],
        gate_passed=True, clean_kills=1)
    out = calibrate.format_summary([(fix_a, res_a), (fix_b, res_b)])
    assert "Per-function" in out
    assert "Per-class aggregate" in out
    assert "Per-mutation aggregate" in out
    assert "mpfr_add_d" in out
    assert "op-swap" in out
    assert "arithmetic" in out and "misc" in out


def test_discover_fixtures_filters_missing_port(tmp_path: Path) -> None:
    db = tmp_path / "state.db"
    _seed_db(db, [("mpfr_add_d", "arithmetic", "done"),
                  ("mpfr_nonexistent_xyzzy", "arithmetic", "done")])
    # eval/functions/mpfr_nonexistent_xyzzy doesn't exist, so it's skipped
    fixtures = calibrate.discover_fixtures(db, REPO_ROOT / "eval" / "functions",
                                           REPO_ROOT, sample_per_class=5)
    names = {f.function for f in fixtures}
    assert "mpfr_add_d" in names
    assert "mpfr_nonexistent_xyzzy" not in names


def test_discover_fixtures_deterministic_with_seed(tmp_path: Path) -> None:
    db = tmp_path / "state.db"
    rows = [(f"mpfr_add{i}_d" if i % 2 else f"mpfr_mul{i}", "arithmetic", "done")
            for i in range(10)]
    _seed_db(db, rows)
    # All these are missing on disk; we test ordering of the sampling logic
    # by stubbing out the existence check via a real fixture that does exist.
    rows2 = [("mpfr_add_d", "arithmetic", "done"),
             ("mpfr_add", "arithmetic", "done"),
             ("mpfr_abs", "arithmetic", "done"),
             ("mpfr_cmp", "arithmetic", "done")]
    db2 = tmp_path / "state2.db"
    _seed_db(db2, rows2)
    a = calibrate.discover_fixtures(db2, REPO_ROOT / "eval" / "functions",
                                     REPO_ROOT, sample_per_class=2, seed=42)
    b = calibrate.discover_fixtures(db2, REPO_ROOT / "eval" / "functions",
                                     REPO_ROOT, sample_per_class=2, seed=42)
    assert [f.function for f in a] == [f.function for f in b]


def test_port_path_resolution() -> None:
    assert calibrate._resolve_port_path("mpfr_add_d", "arithmetic", REPO_ROOT) == \
        REPO_ROOT / "src" / "ops" / "add_d.ts"
    assert calibrate._resolve_port_path("mpfr_mpn_cmp_aux", "substrate", REPO_ROOT) == \
        REPO_ROOT / "src" / "internal" / "mpfr" / "mpn_cmp_aux.ts"
    assert calibrate._resolve_port_path("mpn_add_n", "substrate", REPO_ROOT) == \
        REPO_ROOT / "src" / "internal" / "mpn" / "add_n.ts"


def test_calibrate_smoke_real_add_d() -> None:
    fix = calibrate.CalibrationFixture("mpfr_add_d", ADD_D_PORT, ADD_D_GOLDEN, "arithmetic")
    paired = calibrate.calibrate([fix], REPO_ROOT)
    assert len(paired) == 1
    f, r = paired[0]
    assert f.function == "mpfr_add_d"
    assert len(r.mutations) > 0
