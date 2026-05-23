---
name: decision-runtime
description: "Bun ≥1.3 for harness + dev loop; published `src/` portable across Bun AND Node ≥22 (no Bun.* or node:* imports in src/)."
metadata: 
  node_type: memory
  type: project
  originSessionId: ae70f34d-7cbf-4f33-9344-c0dbf83add5c
---

**Decision:** The harness and developer loop run on **Bun ≥1.3**. The
published library (`src/`, `src/internal/`) runs on **either Bun or
Node ≥22** with no code changes.

**Why:** User asked "any reason not to use Bun?" Reasons that came
up in the analysis:

- Wins: native TS execution (no flag), faster startup, built-in
  `bun test`, faster subprocess via `Bun.spawn`, consistent with
  user's [[reference-auto-port-eval]] sibling project conventions
  ([[user-profile]] runs Bun in `../scientist-workbench`).
- No blocker for the harness: Bun's Worker API has `worker.terminate()`
  with the same semantics we need for [[project-mpfr-goal]]'s Rule 4
  (per-test infinite-loop containment).
- Constraint that *did* surface: the library should not be Bun-locked.
  Most consumers of an MPFR replacement will be on Node. The fix is
  a runtime split, not abandoning Bun.

**How to apply:**

- **`src/` and `src/internal/`** — pure ESM + native `BigInt` only.
  No `Bun.*` calls. No `node:*` imports. No third-party packages.
  Test on Node ≥22 with `node --experimental-strip-types` as a
  smoke check before declaring a port done.
- **`eval/`** — Bun-native. Use `Bun.spawn`, `Bun.file`, Bun
  `Worker` (the standard Web Worker API, which Bun implements),
  `bun test` for integration suites.
- **`package.json`** declares engine compatibility for both Bun ≥1.3
  and Node ≥22; lists zero runtime deps.
- **CI-equivalent local check** — `bun eval/harness/runner.ts ...`
  AND `node --experimental-strip-types eval/harness/runner.ts ...`
  should produce byte-identical grade.json on the same port.
  Divergence is a bug in either the port (Bun-only API leaked) or
  the harness (assumed Bun semantics that Node lacks).
- **Python driver scripts** (`ralph.py`, `dashboard.py`,
  `mutation_prove.py`) call into the harness via subprocess and
  don't care which TS runtime is used.

Captured as Rule 12 in CLAUDE.md ("No npm dependencies in the port;
port runs on Bun OR Node") and the "Practical guidance" section.
