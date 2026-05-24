"""mutate.py — orchestrate mutators + grader to mutation-prove a port
(CLAUDE.md PIL.3). For each applicable mutation: apply via
eval/driver/mutators.ts, grade via eval/harness/runner.ts with a wall
timeout, read composite_correctness from grade.json. Gate passes when
at least one applied mutant scored <= 0.95. Tmp files cleaned in a
try/finally. All subprocess calls run with cwd=repo_root to sidestep
the bunfig.toml preload bug (bd mpfr-ts-6s9); stderr is captured so
grader stack traces surface in test failures.

Ref: CLAUDE.md PIL.3, Rule 4; eval/driver/mutators.ts; eval/harness/runner.ts.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

BELOW_THRESHOLD = 0.95  # PIL.3
CLEAN_KILL = 0.55       # heuristic: composite this low = mutator killed it cleanly
DEFAULT_TIMEOUT_S = 30.0

# Matches a quoted relative-import target after `from `: './x.ts' or '../y.ts'.
# Anchoring on `from ` covers `import ... from`, `import type ... from`, and
# re-export `} from` forms without matching string literals in code bodies.
_RELIMPORT_RE = re.compile(r"""(?<=from )(['"])(\.\.?/[^'"]+)\1""")

# first_error substrings that mean the mutant never actually ran. Compared
# case-insensitively against grade.json's `first_error` field.
_INIT_FAIL_PATTERNS = (
    "port init exceeded",
    "import failed",
    "module init",
    "cannot find module",
)


@dataclass(frozen=True)
class MutationOutcome:
    name: str
    composite: float | None        # None if mutator returned not-applicable, grader timed out / failed, or module init failed
    below_threshold: bool          # composite <= 0.95
    clean_kill: bool               # composite <= 0.55
    module_init_failed: bool       # True if grader's first_error matches an init-failure signature


@dataclass(frozen=True)
class ProveResult:
    function_name: str
    mutations: list[MutationOutcome]
    gate_passed: bool              # >= 1 applied non-init-failed mutation has below_threshold
    clean_kills: int


def _aggregate_gate(outcomes: list[MutationOutcome]) -> bool:
    return any(m.below_threshold for m in outcomes if not m.module_init_failed)


def _rewrite_relative_imports(text: str, port_dir: Path) -> str:
    """Rewrite relative import specifiers to absolute paths anchored at the
    original port's directory. Used when materializing a mutant in /tmp where
    `./` / `../` no longer resolve. Bare-package imports are left untouched."""
    base = port_dir.resolve()

    def _sub(m: re.Match[str]) -> str:
        quote, rel = m.group(1), m.group(2)
        return f"{quote}{(base / rel).resolve()}{quote}"

    return _RELIMPORT_RE.sub(_sub, text)


def _detect_module_init_failed(first_error: object) -> bool:
    if not isinstance(first_error, str):
        return False
    low = first_error.lower()
    return any(p in low for p in _INIT_FAIL_PATTERNS)


def _list_applicable(port_path: Path, repo_root: Path) -> list[str]:
    """Invoke `bun mutators.ts list --input <port>`; return mutation names."""
    proc = subprocess.run(
        ["bun", "eval/driver/mutators.ts", "list", "--input", str(port_path)],
        cwd=str(repo_root),
        capture_output=True,
        text=True,
        check=True,
    )
    return [line.strip() for line in proc.stdout.splitlines() if line.strip()]


def _apply_mutation(
    port_path: Path, mutant_path: Path, mutation: str, repo_root: Path
) -> bool:
    """Apply one mutation, then rewrite relative imports in the materialized
    mutant so it resolves neighbours via the original port's directory.
    Returns True if applied, False if exit code 3 (not applicable)."""
    proc = subprocess.run(
        ["bun", "eval/driver/mutators.ts", "apply",
         "--input", str(port_path),
         "--output", str(mutant_path),
         "--mutation", mutation],
        cwd=str(repo_root),
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode == 0:
        text = mutant_path.read_text(encoding="utf-8")
        rewritten = _rewrite_relative_imports(text, port_path.parent)
        if rewritten != text:
            mutant_path.write_text(rewritten, encoding="utf-8")
        return True
    if proc.returncode == 3:
        return False
    raise RuntimeError(
        f"mutators.ts apply {mutation} exited {proc.returncode}: "
        f"stdout={proc.stdout!r} stderr={proc.stderr!r}"
    )


def _grade_mutant(
    function_name: str,
    mutant_path: Path,
    golden_path: Path,
    grade_path: Path,
    repo_root: Path,
    timeout_s: float,
) -> tuple[float | None, object]:
    """Grade one mutant. Returns (composite_correctness, first_error). On
    timeout / JSON-decode failure / missing field, composite is None and
    first_error is None."""
    try:
        subprocess.run(
            ["bun", "eval/harness/runner.ts",
             "--function", function_name,
             "--port", str(mutant_path),
             "--golden", str(golden_path),
             "--output", str(grade_path)],
            cwd=str(repo_root),
            capture_output=True,
            text=True,
            timeout=timeout_s,
            check=False,
        )
    except subprocess.TimeoutExpired:
        return None, None
    if not grade_path.exists():
        return None, None
    try:
        data = json.loads(grade_path.read_text())
    except json.JSONDecodeError:
        return None, None
    val = data.get("composite_correctness")
    err = data.get("first_error")
    if not isinstance(val, (int, float)):
        return None, err
    return float(val), err


def mutation_prove(
    function_name: str,
    port_path: Path,
    golden_path: Path,
    *,
    repo_root: Path,
    tmp_dir: Path | None = None,
    grader_timeout_s: float = DEFAULT_TIMEOUT_S,
) -> ProveResult:
    """Mutate-and-grade a port; report which mutations broke the golden."""
    owned_tmp = tmp_dir is None
    work_dir = Path(tempfile.mkdtemp(prefix="mpfr_mutate_")) if owned_tmp else tmp_dir
    outcomes: list[MutationOutcome] = []
    try:
        applicable = _list_applicable(port_path, repo_root)
        for name in applicable:
            mutant = work_dir / f"mutant_{function_name}_{name}.ts"
            grade = work_dir / f"grade_{function_name}_{name}.json"
            applied = _apply_mutation(port_path, mutant, name, repo_root)
            if not applied:
                outcomes.append(MutationOutcome(name=name, composite=None,
                                                below_threshold=False, clean_kill=False,
                                                module_init_failed=False))
                continue
            composite, first_error = _grade_mutant(
                function_name, mutant, golden_path, grade,
                repo_root, grader_timeout_s,
            )
            init_failed = _detect_module_init_failed(first_error)
            if init_failed:
                # Mutant failed at import time — composite is vacuous, discard.
                composite = None
            below = composite is not None and composite <= BELOW_THRESHOLD
            clean = composite is not None and composite <= CLEAN_KILL
            outcomes.append(MutationOutcome(name=name, composite=composite,
                                            below_threshold=below, clean_kill=clean,
                                            module_init_failed=init_failed))
        gate = _aggregate_gate(outcomes)
        kills = sum(1 for m in outcomes if m.clean_kill)
        return ProveResult(function_name=function_name, mutations=outcomes,
                           gate_passed=gate, clean_kills=kills)
    finally:
        # Always clean: remove mutant_*.ts and grade_*.json in the work
        # dir; if we own the dir entirely, drop the whole tree.
        if owned_tmp:
            shutil.rmtree(work_dir, ignore_errors=True)
        else:
            for p in list(work_dir.glob("mutant_*.ts")) + list(work_dir.glob("grade_*.json")):
                try:
                    p.unlink()
                except OSError:
                    pass


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="Mutation-prove a port against its golden.")
    ap.add_argument("--function", required=True)
    ap.add_argument("--port", required=True, type=Path)
    ap.add_argument("--golden", required=True, type=Path)
    ap.add_argument("--tmp-dir", type=Path, default=None)
    ap.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    ap.add_argument("--grader-timeout-s", type=float, default=DEFAULT_TIMEOUT_S)
    args = ap.parse_args(argv)
    r = mutation_prove(args.function, args.port, args.golden,
                       repo_root=args.repo_root, tmp_dir=args.tmp_dir,
                       grader_timeout_s=args.grader_timeout_s)
    print(f"function: {r.function_name}")
    print(f"gate_passed: {r.gate_passed}  clean_kills: {r.clean_kills}")
    for m in r.mutations:
        c = "None" if m.composite is None else f"{m.composite:.4f}"
        print(f"  {m.name:<18} composite={c:<8} below={m.below_threshold} "
              f"clean={m.clean_kill} init_failed={m.module_init_failed}")
    return 0 if r.gate_passed else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
