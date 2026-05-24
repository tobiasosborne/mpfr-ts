"""Diff gen_spec output against curated spec.json files.

Read-only verification: for every curated `eval/functions/<fn>/spec.json`,
look up the C source via callgraph.json, run `gen_spec.extract_spec`, and
compare the structural fields. Reports diffs; never modifies anything.

CLI: python3 eval/driver/validate_specs.py [--summary | --json | --function FN]
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import gen_spec

_FIELDS = ("class", "signature.params", "signature.returns", "c_signature", "prec_unit")


@dataclass(frozen=True)
class SpecFieldDiff:
    field: str
    gen_spec_value: Any
    curated_value: Any
    matches: bool


@dataclass(frozen=True)
class SpecValidationResult:
    function: str
    c_source: Path | None
    curated_spec: Path
    diffs: list[SpecFieldDiff] = field(default_factory=list)
    extraction_error: str | None = None


def _normalize_signature(s: str) -> str:
    return re.sub(r"\s+", " ", s).strip()


def _get(d: dict, dotted: str) -> Any:
    cur: Any = d
    for k in dotted.split("."):
        if not isinstance(cur, dict) or k not in cur:
            return None
        cur = cur[k]
    return cur


def _compare(name: str, a: Any, b: Any) -> bool:
    if name == "c_signature" and isinstance(a, str) and isinstance(b, str):
        return _normalize_signature(a) == _normalize_signature(b)
    return a == b


def _err(fn: str, c: Path | None, spec: Path, msg: str) -> SpecValidationResult:
    return SpecValidationResult(fn, c, spec, [], msg)


def validate_spec(c_source: Path, function: str, curated_spec: Path) -> SpecValidationResult:
    """Run gen_spec and diff against the curated spec. Read-only."""
    try:
        curated = json.loads(curated_spec.read_text())
    except (OSError, json.JSONDecodeError) as e:
        return _err(function, c_source, curated_spec, f"curated spec unreadable: {e}")
    if not c_source.exists():
        return _err(function, None, curated_spec, f"{c_source} not found")
    try:
        gen = gen_spec.extract_spec(c_source, function)
    except Exception as e:  # noqa: BLE001
        return _err(function, c_source, curated_spec, str(e))
    diffs = []
    for f in _FIELDS:
        g, c = _get(gen, f), _get(curated, f)
        diffs.append(SpecFieldDiff(f, g, c, _compare(f, g, c)))
    return SpecValidationResult(function, c_source, curated_spec, diffs)


def validate_all_specs(
    eval_functions_dir: Path, mpfr_src_dir: Path, callgraph_path: Path,
) -> list[SpecValidationResult]:
    """Walk eval/functions/, look up each function's C source, validate each."""
    try:
        cg = json.loads(callgraph_path.read_text()).get("functions", {})
    except (OSError, json.JSONDecodeError) as e:
        print(f"warning: callgraph unreadable: {e}", file=sys.stderr)
        return []
    results: list[SpecValidationResult] = []
    for fn_dir in sorted(eval_functions_dir.iterdir()):
        spec = fn_dir / "spec.json"
        if not fn_dir.is_dir() or not spec.exists():
            continue
        fn = fn_dir.name
        try:
            json.loads(spec.read_text())
        except (OSError, json.JSONDecodeError) as e:
            print(f"warning: skipping {fn}: malformed spec.json ({e})", file=sys.stderr)
            continue
        entry = cg.get(fn)
        if entry is None or not entry.get("defined_in"):
            reason = "not in callgraph" if entry is None else "no defined_in"
            print(f"warning: skipping {fn}: {reason}", file=sys.stderr)
            continue
        results.append(validate_spec(mpfr_src_dir / entry["defined_in"], fn, spec))
    return results


def _row(r: SpecValidationResult) -> str:
    if r.extraction_error is not None:
        return f"| {r.function} | (error) | | | | | ERR: {r.extraction_error} |"
    by = {d.field: d for d in r.diffs}
    cells = ["OK" if by[f].matches else f"DIFF ({by[f].gen_spec_value!r} vs {by[f].curated_value!r})"
             for f in _FIELDS]
    status = "OK" if all(d.matches for d in r.diffs) else "DIFF"
    return f"| {r.function} | {' | '.join(cells)} | {status} |"


def _summary(results: list[SpecValidationResult]) -> str:
    lines = ["| function | class | params | returns | c_signature | prec_unit | status |",
             "| --- | --- | --- | --- | --- | --- | --- |"]
    lines.extend(_row(r) for r in results)
    total = len(results)
    err = sum(1 for r in results if r.extraction_error)
    ok = sum(1 for r in results
             if r.extraction_error is None and all(d.matches for d in r.diffs))
    lines.append("")
    lines.append(f"total={total} ok={ok} diff={total - err - ok} err={err}")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    repo = Path(__file__).resolve().parents[2]
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--summary", action="store_true", default=True)
    g.add_argument("--json", action="store_true")
    ap.add_argument("--function", default=None)
    args = ap.parse_args(argv)
    eval_fns = repo / "eval" / "functions"
    mpfr_src = repo / "mpfr" / "src"
    cg = repo / "eval" / "driver" / "callgraph.json"
    if args.function:
        try:
            entry = json.loads(cg.read_text()).get("functions", {}).get(args.function)
        except (OSError, json.JSONDecodeError) as e:
            print(f"error: {e}", file=sys.stderr); return 2
        if entry is None:
            print(f"error: {args.function} not in callgraph", file=sys.stderr); return 2
        results = [validate_spec(mpfr_src / entry["defined_in"], args.function,
                                 eval_fns / args.function / "spec.json")]
    else:
        results = validate_all_specs(eval_fns, mpfr_src, cg)
    if args.json:
        print(json.dumps([dataclasses.asdict(r) for r in results], default=str, indent=2))
    else:
        print(_summary(results))
    return 0


if __name__ == "__main__":
    sys.exit(main())
