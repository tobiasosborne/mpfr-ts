"""PORT-step driver: drive DeepSeek-V4-Flash (via opencode) to produce
a TypeScript port for an mpfr function, then materialise port.ts in a
canonical location for the grader.

Built on top of opencode_runner.py (Phase 2). This script handles:

  1. Prompt generation (from prompts.build_prompt) or pass-through.
  2. Subprocess invocation of opencode_runner.py.
  3. Two-branch port.ts recovery:
       a. Happy path  -- agent used the Write tool with cwd=out_dir,
          so <out-dir>/port.ts already exists.
       b. Write-tool recovery -- agent wrote to some other path; we
          walk the opencode session DB for tool='write' parts and
          pull the most-likely port content into <out-dir>/port.ts.
  4. ASCII / Cyrillic-homoglyph guard (Rule 13).
  5. Cost estimation with Flash pricing
       ($0.14 / MTok input, $0.28 / MTok output).
  6. Stdout summary line for ralph-loop consumers.

CLI:
  python3 run_deepseek_port.py --fn mpfr_add
  python3 run_deepseek_port.py --prompt-file p.txt --fn-label custom

Exits:
  0  success (port.ts produced and ASCII-clean)
  2  no port.ts found and no recoverable Write tool invocation
  3  port.ts contains non-ASCII bytes
"""

from __future__ import annotations

import argparse
import json
import sqlite3
import subprocess
import sys
import time
import uuid
from pathlib import Path

DRIVER_DIR = Path(__file__).resolve().parent
REPO_ROOT = DRIVER_DIR.parent.parent
OPENCODE_RUNNER = DRIVER_DIR / "opencode_runner.py"
OPENCODE_DB = Path.home() / ".local/share/opencode/opencode.db"

# Flash pricing (post-promo, stable).
FLASH_USD_PER_MTOK_INPUT = 0.14
FLASH_USD_PER_MTOK_OUTPUT = 0.28


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


# Safe typographic Unicode -> ASCII normalization map. Restricted to a
# small fixed whitelist of characters that (a) appear regularly in LLM
# prose / JSDoc output, (b) carry no semantic weight in valid TS source
# (comments only), and (c) cannot create homoglyph stealth-corruption of
# identifiers or literals. Dangerous codepoints (Cyrillic, Greek,
# fullwidth Latin, etc.) are deliberately NOT in this map so the strict
# check_non_ascii() guard continues to reject them post-normalize.
SAFE_UNICODE_NORMALIZATION = {
    "–": "-",     # EN DASH
    "—": "--",    # EM DASH
    "‘": "'",     # LEFT SINGLE QUOTATION MARK
    "’": "'",     # RIGHT SINGLE QUOTATION MARK
    "“": '"',     # LEFT DOUBLE QUOTATION MARK
    "”": '"',     # RIGHT DOUBLE QUOTATION MARK
    "…": "...",   # HORIZONTAL ELLIPSIS
    " ": " ",     # NO-BREAK SPACE
    "§": "S",     # SECTION SIGN
    "·": ".",     # MIDDLE DOT
}


def normalize_safe_unicode(text: str) -> str:
    """Convert the typographic safe-set Unicode to ASCII equivalents.

    See SAFE_UNICODE_NORMALIZATION for the exhaustive list. Other
    non-ASCII bytes (Cyrillic letters etc.) pass through unchanged and
    will be caught by the strict check_non_ascii() guard."""
    if not text:
        return text
    out = text
    for src, dst in SAFE_UNICODE_NORMALIZATION.items():
        if src in out:
            out = out.replace(src, dst)
    return out


def check_non_ascii(text: str) -> list[int] | None:
    """Return a list of 1-based line numbers containing non-ASCII bytes,
    or None if the text is pure ASCII. Used to enforce Rule 13."""
    bad: list[int] = []
    for idx, line in enumerate(text.splitlines(), start=1):
        try:
            line.encode("ascii")
        except UnicodeEncodeError:
            bad.append(idx)
    return bad or None


def estimate_cost_usd(*, input_tokens: int, output_tokens: int) -> float:
    """Flash-pricing USD estimate from token counts."""
    return (
        input_tokens * FLASH_USD_PER_MTOK_INPUT
        + output_tokens * FLASH_USD_PER_MTOK_OUTPUT
    ) / 1_000_000.0


def _session_writes(session_id: str) -> list[dict]:
    """Return chronologically-ordered write/edit tool invocations from a
    session. Mirrors auto-port-eval/recover_flash_writes.py."""
    if not OPENCODE_DB.exists():
        return []
    con = sqlite3.connect(str(OPENCODE_DB))
    try:
        rows = con.execute(
            "SELECT data FROM part WHERE session_id = ? ORDER BY time_created",
            (session_id,),
        ).fetchall()
    finally:
        con.close()
    writes: list[dict] = []
    for (raw,) in rows:
        try:
            d = json.loads(raw)
        except json.JSONDecodeError:
            continue
        if d.get("type") not in ("tool", "tool-invocation"):
            continue
        if d.get("tool") not in ("write", "edit"):
            continue
        state = d.get("state") or {}
        inp = state.get("input") or {}
        content = inp.get("content") or inp.get("newString") or ""
        file_path = inp.get("filePath") or ""
        if content and file_path:
            writes.append({"filePath": file_path, "content": content})
    return writes


def _pick_port_content(writes: list[dict], out_dir: Path) -> str | None:
    """Choose the write that most plausibly *is* the port.

    Strategy:
      1. Last write whose filePath matches <out_dir>/port.ts exactly.
      2. Last write whose filePath ends with /port.ts.
      3. Last write whose filePath ends with .ts and contains 'export'.
      4. Last write overall (best-effort fallback).
    """
    if not writes:
        return None
    target = str((out_dir / "port.ts").resolve())
    for w in reversed(writes):
        try:
            if str(Path(w["filePath"]).resolve()) == target:
                return w["content"]
        except (OSError, ValueError):
            pass
    for w in reversed(writes):
        if w["filePath"].endswith("/port.ts") or w["filePath"].endswith("port.ts"):
            return w["content"]
    for w in reversed(writes):
        if w["filePath"].endswith(".ts") and "export" in w["content"]:
            return w["content"]
    return writes[-1]["content"] if writes else None


def _load_prompt(args: argparse.Namespace) -> tuple[str, str, Path]:
    """Return (prompt_text, fn_label, out_dir).

    Validates that either --fn or (--prompt-file + --fn-label) is set."""
    if args.prompt_file:
        if not args.fn_label:
            sys.stderr.write(
                "--fn-label is required when --prompt-file is set\n"
            )
            sys.exit(64)
        prompt_text = Path(args.prompt_file).read_text()
        fn_label = args.fn_label
    else:
        if not args.fn:
            sys.stderr.write("either --fn or --prompt-file is required\n")
            sys.exit(64)
        # Lazy import so the offline tests don't have to load prompts.py
        # (and its mpfr-spec dependencies) just to import this module.
        from prompts import build_prompt
        prompt_text = build_prompt(args.fn, repo_root=REPO_ROOT)
        fn_label = args.fn

    out_dir = Path(args.out_dir) if args.out_dir else Path(f"/tmp/eval_{fn_label}")
    return prompt_text, fn_label, out_dir


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--fn", help="function name; resolves via prompts.build_prompt")
    ap.add_argument("--prompt-file", help="use this prompt verbatim (requires --fn-label)")
    ap.add_argument("--fn-label", help="label for run-id and naming when --prompt-file is set")
    ap.add_argument("--model", default="deepseek-anthropic/deepseek-v4-flash")
    ap.add_argument("--effort", default="L3", choices=["L1", "L2", "L3"])
    ap.add_argument("--out-dir", default=None)
    args = ap.parse_args()

    prompt_text, fn_label, out_dir = _load_prompt(args)
    out_dir.mkdir(parents=True, exist_ok=True)

    # Persist the prompt for debugging / reproducibility.
    prompt_path = out_dir / "prompt.txt"
    prompt_path.write_text(prompt_text)

    run_id = f"port-{fn_label}-{uuid.uuid4().hex[:8]}"

    cmd = [
        sys.executable,
        str(OPENCODE_RUNNER),
        "--run-id", run_id,
        "--model", args.model,
        "--effort", args.effort,
        "--prompt-file", str(prompt_path),
        "--out-dir", str(out_dir),
    ]
    t0 = time.time()
    cp = subprocess.run(cmd, capture_output=True, text=True)
    wall = time.time() - t0

    if cp.returncode != 0:
        sys.stderr.write(
            f"opencode_runner exited {cp.returncode}\n"
            f"STDOUT:\n{cp.stdout}\nSTDERR:\n{cp.stderr}\n"
        )
        sys.exit(cp.returncode)

    # Load session.json (opencode_runner always emits it on success).
    session_path = out_dir / "session.json"
    if not session_path.exists():
        sys.stderr.write(f"session.json not produced in {out_dir}\n")
        sys.exit(2)
    info = json.loads(session_path.read_text())

    # --- Branch A: happy path -- agent wrote port.ts under out_dir.
    port_path = out_dir / "port.ts"
    recovered = False
    if not port_path.exists():
        # --- Branch B: walk session DB for Write tool invocations.
        sid = info.get("session_id")
        writes = _session_writes(sid) if sid else []
        content = _pick_port_content(writes, out_dir)
        if content is None:
            sys.stderr.write(
                "no port.ts produced and no Write tool invocation found "
                f"in session {sid!r}; raw.txt is at {out_dir / 'raw.txt'}\n"
            )
            sys.exit(2)
        port_path.write_text(content)
        recovered = True
        sys.stderr.write(
            f"[recovery] pulled port.ts ({len(content)} bytes) from "
            f"Write tool side-channel in session {sid}\n"
        )

    # --- ASCII / Cyrillic homoglyph guard (Rule 13).
    # Normalize the typographic safe-set (em-dash etc.) FIRST so we don't
    # reject ports for cosmetic JSDoc Unicode; THEN apply the strict
    # check, which still rejects dangerous homoglyphs (Cyrillic, Greek).
    port_text_raw = port_path.read_text()
    port_text = normalize_safe_unicode(port_text_raw)
    if port_text != port_text_raw:
        port_path.write_text(port_text)
        sys.stderr.write(
            "[normalize] converted safe-set typographic Unicode to ASCII\n"
        )
    bad_lines = check_non_ascii(port_text)
    if bad_lines is not None:
        sys.stderr.write(
            f"port.ts contains non-ASCII bytes on lines: {bad_lines}\n"
            "(Rule 13: ASCII-only; reject and require regeneration.)\n"
        )
        sys.exit(3)

    # --- Cost estimate.
    in_tok = int(info.get("input_tokens") or 0)
    out_tok = int(info.get("output_tokens") or 0)
    usd_est = estimate_cost_usd(input_tokens=in_tok, output_tokens=out_tok)
    cost = {
        "model": args.model,
        "effort": args.effort,
        "input_tokens": in_tok,
        "output_tokens": out_tok,
        "usd_est": usd_est,
        "wall_seconds": wall,
        "recovered_from_write_tool": recovered,
        "run_id": run_id,
    }
    (out_dir / "cost.json").write_text(json.dumps(cost, indent=2))

    # --- Summary line. composite=PENDING because grading is a separate
    # step (ralph.py --grade); this driver is the PORT step only.
    print(
        f"port={port_path} composite=PENDING wall={wall:.1f}s "
        f"tokens={in_tok}+{out_tok} usd_est=${usd_est:.4f}"
        + (" recovered=1" if recovered else "")
    )


if __name__ == "__main__":
    main()
