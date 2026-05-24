"""Tests for eval/driver/mutate.py — mutation-prove orchestrator.

Calibration tests run the real grader against real fixtures, so each
takes a few seconds. That is the point (PIL.3): a golden is only proven
when a mutated port actually drops below 0.95. Run with:
    pytest eval/driver/tests/test_mutate.py -v
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

DRIVER_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(DRIVER_DIR))

import mutate  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[3]
ADD_D_PORT = REPO_ROOT / "src" / "ops" / "add_d.ts"
ADD_D_GOLDEN = REPO_ROOT / "eval" / "functions" / "mpfr_add_d" / "golden.jsonl"
SQR_2_PORT = REPO_ROOT / "src" / "ops" / "sqr_2.ts"
SQR_2_GOLDEN = REPO_ROOT / "eval" / "functions" / "mpfr_sqr_2" / "golden.jsonl"


def _print_calibration(result: "mutate.ProveResult", capsys: pytest.CaptureFixture) -> None:
    with capsys.disabled():
        print(f"\n[calibration {result.function_name}] gate_passed={result.gate_passed} "
              f"clean_kills={result.clean_kills}")
        for m in result.mutations:
            print(f"  {m.name}: composite={m.composite} "
                  f"below_threshold={m.below_threshold} clean_kill={m.clean_kill} "
                  f"module_init_failed={m.module_init_failed}")


def test_calibration_add_d(capsys: pytest.CaptureFixture) -> None:
    """Calibration vs production mpfr_add_d (measured 2026-05-24).

    op-swap=0.0000 (clean kill); rnd-swap=1.0000 (RNDN/RNDZ often agree);
    ternary-negate=1.0000 (most golden cases have ternary=0; negating 0
    is still 0). Gate passes via op-swap's clean kill; the other two are
    'applicable but ineffective' - known regex-perturbation limits, not
    mutator bugs. Menu strengthening tracked separately.
    """
    result = mutate.mutation_prove("mpfr_add_d", ADD_D_PORT, ADD_D_GOLDEN, repo_root=REPO_ROOT)
    _print_calibration(result, capsys)
    assert {m.name for m in result.mutations} == {"op-swap", "rnd-swap", "ternary-negate"}
    for m in result.mutations:
        assert m.module_init_failed is False, (
            f"mutant {m.name} failed at module-init - relative imports unresolved in /tmp"
        )
    assert result.gate_passed is True
    assert result.clean_kills >= 1


def test_calibration_sqr_2(capsys: pytest.CaptureFixture) -> None:
    """Calibration vs production mpfr_sqr_2 (measured 2026-05-24).

    rnd-swap=0.8930 (weak kill - below 0.95 but not clean);
    ternary-negate=1.0000 (same exact-result reason as add_d);
    sign-flip=1.0000 (only touches special-case branches, not normal-
    path arithmetic). Gate passes via rnd-swap clearing the 0.95 bar
    without a clean kill. Menu strengthening tracked separately.
    """
    result = mutate.mutation_prove("mpfr_sqr_2", SQR_2_PORT, SQR_2_GOLDEN, repo_root=REPO_ROOT)
    _print_calibration(result, capsys)
    assert {m.name for m in result.mutations} == {"rnd-swap", "ternary-negate", "sign-flip"}
    for m in result.mutations:
        assert m.module_init_failed is False, (
            f"mutant {m.name} failed at module-init - relative imports unresolved in /tmp"
        )
    assert result.gate_passed is True
    assert result.clean_kills >= 0
    assert any(m.below_threshold for m in result.mutations), (
        "expected at least one mutation to clear the 0.95 threshold"
    )


def test_mutant_imports_resolved(tmp_path: Path) -> None:
    """A port with a relative `../core.ts` import must still init in /tmp after mutation."""
    # Colocate synthetic port with src/ops/*.ts so its `../core.ts` resolves.
    port_path = REPO_ROOT / "src" / "ops" / "_mutate_synth_port.ts"
    port_path.write_text(
        'import { type MPFR, type Result, type RoundingMode } from "../core.ts";\n'
        'export function _mutate_synth(b: MPFR, c: number, p: bigint, r: RoundingMode): Result {\n'
        '  return { value: b, ternary: 1 };\n}\n', encoding="utf-8")
    golden = tmp_path / "synth_golden.jsonl"
    m53 = '{"kind":"normal","sign":1,"prec":"53","exp":"1","mant":"4503599627370496"}'
    golden.write_text(
        '{"tag":"happy","inputs":{"b":' + m53 + ',"c":"1.0","prec":"53","rnd":"RNDN"},'
        '"output":{"value":' + m53 + ',"ternary":1},"time_ns":1000}\n', encoding="utf-8")
    try:
        result = mutate.mutation_prove("_mutate_synth", port_path, golden,
                                       repo_root=REPO_ROOT, tmp_dir=tmp_path)
    finally:
        port_path.unlink(missing_ok=True)
    for m in result.mutations:
        assert m.module_init_failed is False, f"mutant {m.name}: module-init failed"


def test_cleanup_on_normal_exit(tmp_path: Path) -> None:
    """After mutation_prove returns, no mutant_* or grade_* files remain in tmp_dir."""
    mutate.mutation_prove("mpfr_add_d", ADD_D_PORT, ADD_D_GOLDEN,
                          repo_root=REPO_ROOT, tmp_dir=tmp_path)
    leftovers = list(tmp_path.glob("mutant_*.ts")) + list(tmp_path.glob("grade_*.json"))
    assert leftovers == [], f"unexpected leftovers: {leftovers}"


def test_cleanup_on_grader_timeout(tmp_path: Path) -> None:
    """grader_timeout_s=0.001 forces every mutant to time out; no exception escapes."""
    result = mutate.mutation_prove(
        "mpfr_add_d", ADD_D_PORT, ADD_D_GOLDEN,
        repo_root=REPO_ROOT, tmp_dir=tmp_path, grader_timeout_s=0.001,
    )
    assert result.gate_passed is False
    assert all(m.composite is None for m in result.mutations)
    leftovers = list(tmp_path.glob("mutant_*.ts")) + list(tmp_path.glob("grade_*.json"))
    assert leftovers == []


def _mo(name: str, comp: float | None, below: bool, clean: bool, init_failed: bool = False):
    return mutate.MutationOutcome(name=name, composite=comp, below_threshold=below,
                                  clean_kill=clean, module_init_failed=init_failed)


def test_gate_passes_with_one_below_threshold() -> None:
    """gate_passed is 'at least one applied, non-init-failed below_threshold'."""
    assert mutate._aggregate_gate([
        _mo("a", None, False, False),
        _mo("b", 0.4, True, True),
        _mo("c", 0.99, False, False),
    ]) is True
    assert mutate._aggregate_gate([
        _mo("a", 0.99, False, False),
        _mo("b", None, False, False),
    ]) is False
    # Init-failed mutations are excluded from the gate even if below_threshold.
    assert mutate._aggregate_gate([_mo("a", None, True, True, init_failed=True)]) is False
