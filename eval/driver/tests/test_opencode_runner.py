"""Integration test for eval/driver/opencode_runner.py.

Drives a real Flash/L1 call through opencode and validates the runner
produces the expected raw.txt + session.json artifacts. Skipped unless
$DEEPSEEK_API_KEY is set, since the call hits a real API.

Run: cd eval/driver && pytest tests/test_opencode_runner.py -v
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

RUNNER_PATH = DRIVER_DIR / "opencode_runner.py"


def _skip_if_no_key() -> None:
    if not os.environ.get("DEEPSEEK_API_KEY"):
        pytest.skip("DEEPSEEK_API_KEY not set; skipping live opencode integration test")


def test_opencode_runner_module_importable() -> None:
    """Cheap sanity check: the runner module imports without error.

    This is the canonical RED indicator before we copy the file from
    auto-port-eval. Once the file exists, this test passes without any
    network/API access, so it's safe to run in every pytest invocation.
    """
    import opencode_runner  # noqa: F401


@pytest.mark.timeout(300)
def test_opencode_runner_flash_l1_smoke(tmp_path: Path) -> None:
    """End-to-end: invoke opencode_runner.py as a subprocess against
    deepseek-v4-flash at L1 effort, asking the model to reply 'HELLO',
    and assert raw.txt + session.json look right.

    Wall time observed in auto-port-eval RESULTS_DEEPSEEK.md: ~90s for
    a typical Flash/L1 call; we allow generous slack with timeout=300.
    """
    _skip_if_no_key()

    prompt_file = tmp_path / "prompt.txt"
    prompt_file.write_text(
        "Respond with exactly the single word HELLO and nothing else. "
        "No quotes, no markdown, no punctuation."
    )

    run_id = f"test-mpfr-{uuid.uuid4().hex[:8]}"
    cmd = [
        sys.executable,
        str(RUNNER_PATH),
        "--run-id", run_id,
        "--model", "deepseek-anthropic/deepseek-v4-flash",
        "--effort", "L1",
        "--prompt-file", str(prompt_file),
        "--out-dir", str(tmp_path),
    ]

    cp = subprocess.run(cmd, capture_output=True, text=True, timeout=270)
    assert cp.returncode == 0, (
        f"opencode_runner exited {cp.returncode}\n"
        f"STDOUT:\n{cp.stdout}\nSTDERR:\n{cp.stderr}"
    )

    raw_path = tmp_path / "raw.txt"
    session_path = tmp_path / "session.json"
    assert raw_path.exists(), f"raw.txt missing in {tmp_path}; stdout={cp.stdout}"
    assert session_path.exists(), f"session.json missing in {tmp_path}"

    raw = raw_path.read_text()
    assert "HELLO" in raw.upper(), f"HELLO not found in raw.txt; content was:\n{raw!r}"
    assert "<usage>" in raw and "</usage>" in raw, (
        f"<usage> block missing in raw.txt; content was:\n{raw!r}"
    )

    info = json.loads(session_path.read_text())
    assert info["input_tokens"] > 0, f"input_tokens not positive: {info!r}"
    assert info["output_tokens"] > 0, f"output_tokens not positive: {info!r}"
