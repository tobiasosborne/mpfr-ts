#!/usr/bin/env bash
# Run all per-function MPFR golden drivers to materialize golden.jsonl.
#
# Iterates over eval/functions/*/ and invokes each built golden_driver,
# redirecting stdout to golden.jsonl. Idempotent by default -- existing
# goldens are skipped unless --force is given. Drivers that have not
# been compiled yet (no golden_driver executable) are warned-and-skipped
# rather than triggering a rebuild; that stays the concern of build.sh.
#
# Senior-eng note: each driver runs under a 60s timeout. A partial
# golden.jsonl from a crashed or timed-out driver is removed so the
# next run retries cleanly -- half-files are worse than missing files
# because the runner has no way to tell them apart.
#
# Sibling of build.sh; same EVAL_ROOT / nullglob / fn_dirs shape.

set -euo pipefail
cd "$(dirname "$0")"

EVAL_ROOT="$(cd .. && pwd)"

force=0
filter=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --force) force=1; shift ;;
    --filter) filter="${2:-}"; shift 2 ;;
    *) echo "usage: $0 [--force] [--filter <fn>]" >&2; exit 2 ;;
  esac
done

shopt -s nullglob
fn_dirs=("$EVAL_ROOT"/functions/*/)
if [[ ${#fn_dirs[@]} -eq 0 ]]; then
  echo "no function directories under $EVAL_ROOT/functions (nothing to run)"
  exit 0
fi

built=0; generated=0; skipped=0; failed=0
for fn_dir in "${fn_dirs[@]}"; do
  fname=$(basename "$fn_dir")
  if [[ -n "$filter" && "$fname" != "$filter" ]]; then
    continue
  fi
  driver="$fn_dir/golden_driver"
  out="$fn_dir/golden.jsonl"
  if [[ ! -x "$driver" ]]; then
    echo "warn: $fname (no built driver, run build.sh first)" >&2
    skipped=$((skipped + 1))
    continue
  fi
  built=$((built + 1))
  if [[ -e "$out" && "$force" -eq 0 ]]; then
    echo "skip $fname (golden exists)"
    skipped=$((skipped + 1))
    continue
  fi
  # Disable -e around the driver: a single driver failure must not
  # abort the whole run; we want the summary across all functions.
  set +e
  timeout 60 "$driver" >"$out" 2>/dev/null
  rc=$?
  set -e
  if [[ $rc -eq 0 ]]; then
    echo "generated $fname"
    generated=$((generated + 1))
  else
    if [[ $rc -eq 124 ]]; then
      reason="timeout after 60s"
    else
      reason="exit $rc"
    fi
    echo "FAILED $fname: $reason" >&2
    rm -f "$out"
    failed=$((failed + 1))
  fi
done

if [[ -n "$filter" && $built -eq 0 && $skipped -eq 0 && $failed -eq 0 ]]; then
  echo "no function matched filter: $filter"
  exit 0
fi

echo "built: $built generated: $generated skipped: $skipped failed: $failed"
[[ $failed -eq 0 ]]
