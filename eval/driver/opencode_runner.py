"""Drive opencode for a single eval run, then extract the model reply + usage.

Usage:
  python3 opencode_runner.py --run-id rXXXX --model deepseek-anthropic/deepseek-v4-pro \
      --effort L1 --prompt-file /tmp/eval_rXXXX/prompt.txt --out-dir /tmp/eval_rXXXX

Writes to <out-dir>:
  raw.txt       - assistant text + <usage> block (matches finalize.py's expected format)
  port.ts       - (for L3 only) the file the agent wrote during TDD; for L1/L2 we just
                  put the extracted TS content of raw.txt here, finalize.py handles fences
  session.json  - full session export (parts + tokens) for debugging

Effort levels map to opencode invocations:
  L1 - single-turn, no tools allowed (we use `--agent` plan-like to disable bash/edit;
       since opencode doesn't have a "no-tools" agent OOTB, we use the prompt to enforce
       and rely on the model's compliance). Output goes to raw.txt only.
  L2 - single-turn, read-only tools. Prompt allows Read/Grep; we don't gate at the
       runtime layer. Output to raw.txt; finalize.py extracts TS.
  L3 - TDD loop. Pass `--dangerously-skip-permissions`. Agent writes port.ts via Write
       tool; we'll copy /tmp/eval_<rid>/port.ts to out-dir/port.ts.
"""
from __future__ import annotations
import argparse
import json
import sqlite3
import subprocess
import sys
import time
from pathlib import Path

OPENCODE_DB = Path.home() / ".local/share/opencode/opencode.db"
OPENCODE_BIN = Path.home() / ".opencode/bin/opencode"


def latest_session_after(t_start_ms: int) -> str | None:
    if not OPENCODE_DB.exists():
        return None
    con = sqlite3.connect(str(OPENCODE_DB))
    try:
        row = con.execute(
            "SELECT id FROM session WHERE time_created >= ? ORDER BY time_created DESC LIMIT 1",
            (t_start_ms,),
        ).fetchone()
        return row[0] if row else None
    finally:
        con.close()


def session_by_title(title: str, t_start_ms: int) -> str | None:
    """Find a session whose title exactly matches and was created after t_start_ms.
    Used for safe lookup when many opencode processes run in parallel."""
    if not OPENCODE_DB.exists():
        return None
    con = sqlite3.connect(str(OPENCODE_DB))
    try:
        row = con.execute(
            "SELECT id FROM session WHERE title = ? AND time_created >= ? "
            "ORDER BY time_created DESC LIMIT 1",
            (title, t_start_ms),
        ).fetchone()
        return row[0] if row else None
    finally:
        con.close()


def extract_session(session_id: str) -> dict:
    """Return assistant text, summed token usage, and wall ms for a session."""
    con = sqlite3.connect(str(OPENCODE_DB))
    try:
        parts = con.execute(
            "SELECT data, time_created FROM part WHERE session_id = ? ORDER BY time_created",
            (session_id,),
        ).fetchall()
        sess = con.execute(
            "SELECT time_created, time_updated FROM session WHERE id = ?",
            (session_id,),
        ).fetchone()
    finally:
        con.close()

    assistant_text_chunks: list[str] = []
    in_tok = out_tok = reasoning_tok = cache_read = cache_write = 0
    n_steps = 0
    n_tool_uses = 0
    # We need to skip the first text part (which is the user's prompt) and only collect
    # text parts that come AFTER a step-start (i.e., assistant responses).
    seen_step_start = False
    for raw_data, _t in parts:
        try:
            d = json.loads(raw_data)
        except json.JSONDecodeError:
            continue
        ptype = d.get("type")
        if ptype == "step-start":
            seen_step_start = True
        elif ptype == "step-finish":
            n_steps += 1
            tk = d.get("tokens") or {}
            in_tok += int(tk.get("input") or 0)
            out_tok += int(tk.get("output") or 0)
            reasoning_tok += int(tk.get("reasoning") or 0)
            cache = tk.get("cache") or {}
            cache_read += int(cache.get("read") or 0)
            cache_write += int(cache.get("write") or 0)
        elif ptype == "text" and seen_step_start:
            txt = d.get("text") or ""
            if txt:
                assistant_text_chunks.append(txt)
        elif ptype == "tool-invocation" or ptype == "tool":
            n_tool_uses += 1
    wall_ms = (sess[1] - sess[0]) if sess else 0
    return {
        "session_id": session_id,
        "assistant_text": "\n".join(assistant_text_chunks).strip(),
        "input_tokens": in_tok,
        "output_tokens": out_tok,
        "reasoning_tokens": reasoning_tok,
        "cache_read_tokens": cache_read,
        "cache_write_tokens": cache_write,
        "total_tokens": in_tok + out_tok + reasoning_tok,
        "n_steps": n_steps,
        "n_tool_uses": n_tool_uses,
        "duration_ms": wall_ms,
    }


def run_opencode(model: str, effort: str, prompt: str, work_dir: Path,
                 title: str | None = None, timeout: int = 900) -> tuple[int, float]:
    """Invoke opencode run synchronously. Returns (exit_code, wall_seconds)."""
    cmd = [str(OPENCODE_BIN), "run", "-m", model, "--format", "json"]
    if title:
        cmd.extend(["--title", title])
    if effort == "L3":
        cmd.append("--dangerously-skip-permissions")
    cmd.append(prompt)
    t0 = time.time()
    cp = subprocess.run(cmd, cwd=str(work_dir), capture_output=True, text=True, timeout=timeout)
    wall = time.time() - t0
    if cp.returncode != 0:
        sys.stderr.write(f"opencode exit {cp.returncode}\nSTDERR:\n{cp.stderr[-1500:]}\n")
    return cp.returncode, wall


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--run-id", required=True)
    ap.add_argument("--model", required=True, help="opencode model spec, e.g. deepseek-anthropic/deepseek-v4-pro")
    ap.add_argument("--effort", required=True, choices=["L1", "L2", "L3"])
    ap.add_argument("--prompt-file", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--work-dir", default=None, help="cwd for opencode (default: out-dir)")
    args = ap.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    work_dir = Path(args.work_dir) if args.work_dir else out_dir
    work_dir.mkdir(parents=True, exist_ok=True)

    prompt = Path(args.prompt_file).read_text()

    t_start_ms = int(time.time() * 1000)
    # Use run_id as the session title for safe parallel lookup
    timeout = 1500 if args.effort == "L3" else 600
    exit_code, wall = run_opencode(
        args.model, args.effort, prompt, work_dir,
        title=args.run_id, timeout=timeout,
    )

    session_id = session_by_title(args.run_id, t_start_ms) or latest_session_after(t_start_ms)
    if not session_id:
        sys.stderr.write("could not locate opencode session\n")
        sys.exit(2)

    info = extract_session(session_id)
    info["wall_seconds_outer"] = wall
    info["exit_code"] = exit_code
    info["model"] = args.model
    info["effort"] = args.effort
    info["run_id"] = args.run_id

    (out_dir / "session.json").write_text(json.dumps(info, indent=2))

    # Build raw.txt in the format finalize.py expects: assistant text + <usage> block.
    # NOTE: prefer the subprocess wall (wall_seconds_outer) over the DB's session
    # time_updated-time_created, which is asynchronously written and not a faithful
    # measure of model wall time (often <1s even for multi-minute runs).
    raw_parts = [info["assistant_text"], ""]
    duration_ms_real = int(wall * 1000)
    usage_lines = [
        f"total_tokens: {info['total_tokens']}",
        f"input_tokens: {info['input_tokens']}",
        f"output_tokens: {info['output_tokens']}",
        f"duration_ms: {duration_ms_real}",
        f"duration_ms_db: {info['duration_ms']}",
    ]
    if info["n_tool_uses"]:
        usage_lines.append(f"tool_uses: {info['n_tool_uses']}")
    raw_parts.append("<usage>\n" + "\n".join(usage_lines) + "\n</usage>")
    (out_dir / "raw.txt").write_text("\n".join(raw_parts))

    print(f"[{args.run_id}] session={session_id} "
          f"text_chars={len(info['assistant_text'])} "
          f"in={info['input_tokens']} out={info['output_tokens']} "
          f"wall={wall:.1f}s steps={info['n_steps']} tools={info['n_tool_uses']}")


if __name__ == "__main__":
    main()
