"""calibrate.py - sample done ports across classes, mutation-prove each,
emit a 3-section markdown report so the orchestrator can read calibration
patterns at a glance.

Step 5 (analyzing output) is a separate, orchestrator-driven step; this
tool only collects data and renders tables.

CLI: python3 eval/driver/calibrate.py [--functions FN ...] [--class CLS]
                                       [--sample-per-class N]
                                       [--summary | --json]

Ref: CLAUDE.md PIL.3; eval/driver/mutate.py.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import random
import sqlite3
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path

import mutate

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DB = REPO_ROOT / "eval" / "state.db"
DEFAULT_EVAL_FNS = REPO_ROOT / "eval" / "functions"


@dataclass(frozen=True)
class CalibrationFixture:
    function: str
    port_path: Path
    golden_path: Path
    class_: str


def _resolve_port_path(function: str, class_: str, repo_root: Path) -> Path:
    """Map (function, class) to its TS port file. mpn_* (no mpfr_ prefix)
    lives in src/internal/mpn/; substrate mpfr_* in src/internal/mpfr/;
    other classes in src/ops/. mpfr_ is stripped; mpn_ is stripped only when
    it is the function's leading token."""
    if class_ == "substrate" and function.startswith("mpn_"):
        return repo_root / "src" / "internal" / "mpn" / f"{function[4:]}.ts"
    short = function[5:] if function.startswith("mpfr_") else function
    sub = "internal/mpfr" if class_ == "substrate" else "ops"
    return repo_root / "src" / sub / f"{short}.ts"


def _build_fixture(function: str, class_: str, eval_functions_dir: Path,
                   repo_root: Path) -> CalibrationFixture | None:
    port = _resolve_port_path(function, class_, repo_root)
    golden = eval_functions_dir / function / "golden.jsonl"
    if not port.exists():
        print(f"warning: skipping {function}: port missing at {port}", file=sys.stderr)
        return None
    if not golden.exists():
        print(f"warning: skipping {function}: golden missing at {golden}", file=sys.stderr)
        return None
    return CalibrationFixture(function, port, golden, class_)


def discover_fixtures(state_db_path: Path, eval_functions_dir: Path,
                       repo_root: Path, *, sample_per_class: int = 2,
                       seed: int = 42) -> list[CalibrationFixture]:
    """Read state.db for status='done' rows, group by class, sample
    sample_per_class deterministically per class. Skips fixtures with missing
    port or golden files. Clamps the sample size when fewer candidates exist."""
    conn = sqlite3.connect(state_db_path)
    try:
        rows = conn.execute(
            "SELECT name, class FROM functions WHERE status='done' ORDER BY name"
        ).fetchall()
    finally:
        conn.close()
    by_class: dict[str, list[str]] = {}
    for name, cls in rows:
        by_class.setdefault(cls, []).append(name)
    rng = random.Random(seed)
    fixtures: list[CalibrationFixture] = []
    for cls in sorted(by_class):
        candidates = by_class[cls]
        n = min(sample_per_class, len(candidates))
        if sample_per_class > len(candidates):
            print(f"warning: class={cls} has {len(candidates)} done fns, "
                  f"requested {sample_per_class}; clamping", file=sys.stderr)
        for fn in sorted(rng.sample(candidates, n)):
            fix = _build_fixture(fn, cls, eval_functions_dir, repo_root)
            if fix is not None:
                fixtures.append(fix)
    return fixtures


def calibrate(fixtures: list[CalibrationFixture], repo_root: Path,
              *, grader_timeout_s: float = 30.0,
              ) -> list[tuple[CalibrationFixture, mutate.ProveResult]]:
    """Sequentially mutation-prove each fixture."""
    return [(fix, mutate.mutation_prove(
        fix.function, fix.port_path, fix.golden_path,
        repo_root=repo_root, grader_timeout_s=grader_timeout_s)) for fix in fixtures]


def _applied(outcomes: list[mutate.MutationOutcome]) -> list[mutate.MutationOutcome]:
    return [m for m in outcomes if m.composite is not None]


def _mean(vals: list[float]) -> str:
    return "n/a" if not vals else f"{statistics.fmean(vals):.2f}"


def _pct(n: int, d: int) -> str:
    return "n/a" if d == 0 else f"{100.0 * n / d:.0f}%"


def format_summary(paired: list[tuple[CalibrationFixture, mutate.ProveResult]]) -> str:
    # Per-function section
    pf = ["## Per-function",
          "| function | class | applicable | clean kills | gate passed | notes |",
          "| --- | --- | --- | --- | --- | --- |"]
    for fix, r in paired:
        applied = _applied(r.mutations)
        names = ", ".join(m.name for m in applied) or "(none)"
        notes = "init-failed mutants present" if any(m.module_init_failed for m in r.mutations) else ""
        pf.append(f"| {fix.function} | {fix.class_} | {names} | {r.clean_kills} | "
                  f"{'yes' if r.gate_passed else 'no'} | {notes} |")
    # Per-class section
    by_class: dict[str, list[tuple[CalibrationFixture, mutate.ProveResult]]] = {}
    for fix, r in paired:
        by_class.setdefault(fix.class_, []).append((fix, r))
    pc = ["", "## Per-class aggregate",
          "| class | n_fns | total applied | total clean kills | mean composite | gate pass rate |",
          "| --- | --- | --- | --- | --- | --- |"]
    for cls in sorted(by_class):
        entries = by_class[cls]
        applied_all = [m for _, r in entries for m in _applied(r.mutations)]
        comps = [m.composite for m in applied_all if m.composite is not None]
        kills = sum(1 for m in applied_all if m.clean_kill)
        gates = sum(1 for _, r in entries if r.gate_passed)
        pc.append(f"| {cls} | {len(entries)} | {len(applied_all)} | {kills} | "
                  f"{_mean(comps)} | {_pct(gates, len(entries))} |")
    # Per-mutation section
    by_mut: dict[str, list[mutate.MutationOutcome]] = {}
    for _, r in paired:
        for m in _applied(r.mutations):
            by_mut.setdefault(m.name, []).append(m)
    pm = ["", "## Per-mutation aggregate",
          "| mutation | times applicable | clean kills | mean composite when applied |",
          "| --- | --- | --- | --- |"]
    for name in sorted(by_mut):
        outs = by_mut[name]
        comps = [m.composite for m in outs if m.composite is not None]
        kills = sum(1 for m in outs if m.clean_kill)
        pm.append(f"| {name} | {len(outs)} | {kills} | {_mean(comps)} |")
    return "\n".join(pf + pc + pm)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Calibrate mutation-prove across sampled ports.")
    ap.add_argument("--functions", nargs="+", default=None,
                    help="explicit function names; overrides discovery")
    ap.add_argument("--class", dest="class_", default=None, help="filter to one class")
    ap.add_argument("--sample-per-class", type=int, default=2)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--grader-timeout-s", type=float, default=30.0)
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--summary", action="store_true", default=True)
    g.add_argument("--json", action="store_true")
    args = ap.parse_args(argv)
    if args.functions:
        conn = sqlite3.connect(DEFAULT_DB)
        try:
            cls_map = dict(conn.execute("SELECT name, class FROM functions").fetchall())
        finally:
            conn.close()
        fixtures = [f for f in (_build_fixture(fn, cls_map.get(fn, "misc"),
                                               DEFAULT_EVAL_FNS, REPO_ROOT)
                                for fn in args.functions) if f is not None]
    else:
        fixtures = discover_fixtures(DEFAULT_DB, DEFAULT_EVAL_FNS, REPO_ROOT,
                                      sample_per_class=args.sample_per_class, seed=args.seed)
        if args.class_:
            fixtures = [f for f in fixtures if f.class_ == args.class_]
    paired = calibrate(fixtures, REPO_ROOT, grader_timeout_s=args.grader_timeout_s)
    if args.json:
        print(json.dumps([{"fixture": {"function": f.function, "class": f.class_,
                                         "port_path": str(f.port_path),
                                         "golden_path": str(f.golden_path)},
                            "result": dataclasses.asdict(r)} for f, r in paired], indent=2))
    else:
        print(format_summary(paired))
    return 0


if __name__ == "__main__":
    sys.exit(main())
