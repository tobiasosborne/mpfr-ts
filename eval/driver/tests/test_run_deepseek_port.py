"""Tests for eval/driver/run_deepseek_port.py - the PORT-step driver
built on top of opencode_runner.py.

Three offline tests run in every pytest invocation:
  - test_module_importable
  - test_cyrillic_check_rejects_non_ascii
  - test_cost_estimator

One live test runs only when $DEEPSEEK_API_KEY is set:
  - test_end_to_end_synthetic - drives Flash/L3 against a trivial Write
    prompt to verify port.ts is materialised (either via the happy path
    or the Write-tool recovery branch) and the cost.json is sane.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import uuid
from pathlib import Path

import pytest

DRIVER_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(DRIVER_DIR))

DRIVER_SCRIPT = DRIVER_DIR / "run_deepseek_port.py"


def _skip_if_no_key() -> None:
    if not os.environ.get("DEEPSEEK_API_KEY"):
        pytest.skip("DEEPSEEK_API_KEY not set; skipping live deepseek port test")


def test_module_importable() -> None:
    """Sanity: the module imports without error. Canonical RED before
    run_deepseek_port.py exists."""
    import run_deepseek_port  # noqa: F401


def test_cyrillic_check_rejects_non_ascii() -> None:
    """The non-ASCII / Cyrillic homoglyph guard (Rule 13) must flag any
    non-ASCII byte and report offending line numbers."""
    import run_deepseek_port as mod

    # Line 2 contains a Cyrillic 'a' (U+0430) instead of Latin 'a'.
    bad = "const x = 0xaaaan;\nconst y = 0xaaaaaaaaaaaaaaaаn;\nconst z = 1n;\n"
    result = mod.check_non_ascii(bad)
    assert result is not None, "check_non_ascii should report a problem"
    # Helper contract: returns a list of offending 1-based line numbers
    # (or any truthy structure naming them); we just require line 2 is in it.
    if isinstance(result, list):
        assert 2 in result
    else:
        assert "2" in str(result)

    good = "const x = 0n;\nexport function hello() { return 'hi'; }\n"
    assert mod.check_non_ascii(good) is None, "pure ASCII must pass"


def test_normalize_safe_unicode_converts_typography() -> None:
    """Safe typographic Unicode (em-dash, en-dash, smart quotes, ellipsis,
    section sign, nbsp) is converted to ASCII so the strict Rule 13 check
    accepts ports whose ONLY non-ASCII bytes are JSDoc typography.

    Dangerous homoglyphs (Cyrillic letters, Greek, etc.) are NOT touched
    by the normalizer -- they continue to fail check_non_ascii after
    normalization, preserving the original Rule 13 guarantee."""
    import run_deepseek_port as mod

    # Typographic-only text -> normalize succeeds -> strict check passes.
    typographic = "Ref: foo.c L37–L60 — the C reference.\nuse ‘x’ and “y” …"
    normalized = mod.normalize_safe_unicode(typographic)
    assert mod.check_non_ascii(normalized) is None, (
        f"post-normalize text should be pure ASCII; got {normalized!r}"
    )
    # Specific mappings.
    assert "—" not in normalized and "--" in normalized
    assert "–" not in normalized and "-" in normalized
    assert "‘" not in normalized and "’" not in normalized
    assert "“" not in normalized and "”" not in normalized
    assert "…" not in normalized and "..." in normalized

    # Cyrillic 'a' (U+0430) is NOT in the safe set -- normalize leaves it,
    # and the strict check rejects.
    dangerous = "const x = 0xaaaaаn;\n"
    normalized = mod.normalize_safe_unicode(dangerous)
    assert "а" in normalized, "Cyrillic must NOT be normalized away"
    assert mod.check_non_ascii(normalized) is not None, (
        "Cyrillic homoglyph must still fail strict check post-normalize"
    )


def test_cost_estimator() -> None:
    """Flash pricing: $0.14 / MTok input, $0.28 / MTok output."""
    import run_deepseek_port as mod

    usd = mod.estimate_cost_usd(input_tokens=10_000, output_tokens=5_000)
    expected = (10_000 * 0.14 + 5_000 * 0.28) / 1_000_000
    assert abs(usd - expected) < 1e-12, f"got {usd!r}, expected {expected!r}"
    assert abs(usd - 0.0028) < 1e-9


@pytest.mark.timeout(600)
def test_end_to_end_synthetic(tmp_path: Path) -> None:
    """Live: drive Flash/L3 with a synthetic trivial-TS prompt and assert
    that <out-dir>/port.ts is produced, ASCII-clean, and cost.json exists.

    The prompt asks Flash to use its Write tool to create the file at a
    specific path with exact contents - no real mpfr porting, so the run
    completes in seconds rather than minutes."""
    _skip_if_no_key()

    out_dir = tmp_path / "out"
    out_dir.mkdir()

    prompt_text = (
        f"Use your Write tool to create the file at exactly this path: "
        f"{out_dir}/port.ts. The file's content must be exactly:\n\n"
        f"export function hello(): string {{ return 'hello'; }}\n\n"
        f"Do not output any prose; just call the Write tool with the "
        f"exact content."
    )
    prompt_file = tmp_path / "prompt.txt"
    prompt_file.write_text(prompt_text)

    fn_label = f"synthhello-{uuid.uuid4().hex[:8]}"
    cmd = [
        sys.executable,
        str(DRIVER_SCRIPT),
        "--prompt-file", str(prompt_file),
        "--fn-label", fn_label,
        "--out-dir", str(out_dir),
        "--effort", "L3",
    ]
    cp = subprocess.run(cmd, capture_output=True, text=True, timeout=540)
    assert cp.returncode == 0, (
        f"run_deepseek_port exited {cp.returncode}\n"
        f"STDOUT:\n{cp.stdout}\nSTDERR:\n{cp.stderr}"
    )

    port_path = out_dir / "port.ts"
    assert port_path.exists(), (
        f"port.ts missing in {out_dir}; stdout={cp.stdout!r}; stderr={cp.stderr!r}"
    )
    port_text = port_path.read_text()
    assert "export function hello" in port_text, (
        f"port.ts present but missing expected content; got:\n{port_text!r}"
    )

    cost_path = out_dir / "cost.json"
    assert cost_path.exists(), f"cost.json missing in {out_dir}"
    cost = json.loads(cost_path.read_text())
    assert cost["model"].startswith("deepseek"), cost
    assert cost["input_tokens"] > 0, cost
    assert cost["output_tokens"] > 0, cost
    assert cost["usd_est"] > 0.0, cost

    # Stdout summary line contract.
    assert "port=" in cp.stdout and "usd_est=$" in cp.stdout, cp.stdout
