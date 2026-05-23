#!/usr/bin/env python3
"""Ralph-loop driver CLI for mpfr-ts (Pilot Step 9 — dry-run only).

Pilot Step 9 covers the *prompt rendering* slice of the ralph loop: this
script can render the prompt that would be handed to a sonnet L3 agent
for a given function, and can enumerate which functions are still
pending in ``eval/state.db``. Live agent dispatch (Step 10) is **not**
implemented here.

CLI surface
-----------
::

    python3 eval/driver/ralph.py [--function FN] [--dry-run] [--list-pending]

- ``--dry-run`` renders the prompt for ``--function FN`` to stdout and
  exits 0. ``--function`` is required in this mode.
- ``--list-pending`` queries ``eval/state.db`` for ``status='pending'``
  functions sorted by ``topo_rank`` and prints one per line.
- Exactly one mode must be selected; selecting both or neither is an
  error.

References
----------
- CLAUDE.md §"Project phase awareness" and §"Pilot gating" (PIL.1–PIL.5).
- docs/PILOT_PLAN.md row 9 — acceptance for this step.
"""

from __future__ import annotations

import argparse
import sqlite3
import sys
from pathlib import Path

from prompts import build_prompt


def _repo_root() -> Path:
    """Repo root inferred from this file's location.

    ``eval/driver/ralph.py`` lives two directories below the repo root.
    Anchoring to ``__file__`` keeps the script working regardless of cwd.
    """
    return Path(__file__).resolve().parents[2]


def _list_pending(db_path: Path) -> int:
    """Print pending functions from ``state.db``, lowest ``topo_rank`` first.

    Returns the process exit code.
    """
    if not db_path.exists():
        print(f"error: state.db not found at {db_path}", file=sys.stderr)
        return 2
    # `uri=True` plus `mode=ro` makes the read truly read-only — we never
    # need to write here and a flag-typo that opened r/w could silently
    # corrupt the canonical tracker.
    uri = f"file:{db_path}?mode=ro"
    with sqlite3.connect(uri, uri=True) as conn:
        cur = conn.execute(
            "SELECT name FROM functions WHERE status = 'pending' "
            "ORDER BY topo_rank, name"
        )
        rows = cur.fetchall()
    for (name,) in rows:
        print(name)
    return 0


def _dry_run(function_name: str, repo_root: Path) -> int:
    """Render the prompt for ``function_name`` to stdout."""
    try:
        prompt = build_prompt(function_name, repo_root=repo_root)
    except FileNotFoundError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    except ValueError as exc:
        print(f"error: malformed spec for {function_name}: {exc}", file=sys.stderr)
        return 2
    # `end=""` keeps stdout byte-identical to the rendered string. The
    # builder already terminates the prompt with a single newline.
    print(prompt, end="")
    return 0


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ralph.py",
        description=(
            "mpfr-ts ralph loop driver. Pilot Step 9 supports prompt "
            "dry-run rendering and state.db queries; live dispatch is "
            "Step 10 and not implemented here."
        ),
    )
    parser.add_argument(
        "--function",
        metavar="FN",
        help="C function name (e.g. mpn_add_n). Required with --dry-run.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Render the full prompt for --function to stdout and exit.",
    )
    parser.add_argument(
        "--list-pending",
        action="store_true",
        help="Print functions with status='pending' from state.db, "
        "one per line, sorted by topo_rank.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)

    if args.dry_run and args.list_pending:
        print("error: --dry-run and --list-pending are mutually exclusive",
              file=sys.stderr)
        return 2
    if not args.dry_run and not args.list_pending:
        print("error: specify one of --dry-run or --list-pending",
              file=sys.stderr)
        return 2

    root = _repo_root()
    if args.list_pending:
        return _list_pending(root / "eval" / "state.db")

    # --dry-run path
    if not args.function:
        print("error: specify --function for dry-run mode", file=sys.stderr)
        return 2
    return _dry_run(args.function, root)


if __name__ == "__main__":
    sys.exit(main())
