#!/usr/bin/env bash
# Build all per-function MPFR golden drivers.
#
# Iterates over eval/functions/*/ and compiles any golden_driver.c
# found. Drivers missing on this checkout are skipped silently — the
# script is safe to run before any driver exists (Step 4 of the pilot
# uses it that way to smoke-test the build pipeline itself).
#
# Senior-eng note: -Werror is mandatory. A warning here means a bug in
# the driver or in common.h; suppressing warnings is how silent
# correctness bugs ship.
#
# Adapted from ../auto-port-eval/eval/golden_master/build.sh — same
# shape, swapped -lflint for -lmpfr -lgmp -lm and added -Werror.

set -euo pipefail
cd "$(dirname "$0")"

EVAL_ROOT="$(cd .. && pwd)"

# pkg-config --cflags --libs mpfr typically returns just `-lmpfr` (no
# extra cflags on Ubuntu) but we honour whatever it emits so this stays
# correct on other distros and on systems with MPFR in non-standard
# prefixes. -lgmp -lm are added explicitly: gmp is mpfr's hard dep but
# libmpfr.pc doesn't always list it (Ubuntu's does, Debian's older
# packaging didn't), and libm we need for the smoke driver itself.
MPFR_FLAGS=$(pkg-config --cflags --libs mpfr 2>/dev/null || echo "-lmpfr")
COMMON_FLAGS="-O2 -std=c11 -Wall -Wextra -Werror -I."
EXTRA_LIBS="-lgmp -lm"

shopt -s nullglob
fn_dirs=("$EVAL_ROOT"/functions/*/)
if [[ ${#fn_dirs[@]} -eq 0 ]]; then
  echo "no function directories under $EVAL_ROOT/functions (nothing to build)"
  exit 0
fi

for fn_dir in "${fn_dirs[@]}"; do
  fname=$(basename "$fn_dir")
  driver="$fn_dir/golden_driver.c"
  if [[ ! -f "$driver" ]]; then
    echo "skip $fname (no driver)"
    continue
  fi
  out="$fn_dir/golden_driver"
  echo "build $fname"
  # shellcheck disable=SC2086  # intentional word-splitting of flag vars
  gcc $COMMON_FLAGS "$driver" $MPFR_FLAGS $EXTRA_LIBS -o "$out"
done
echo "done."
