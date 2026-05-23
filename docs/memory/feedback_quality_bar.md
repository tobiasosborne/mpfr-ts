---
name: feedback-quality-bar
description: "When in doubt: what would a senior TS expert who is uncompromising demand? Plus: spawn a review subagent after substantial work."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: ae70f34d-7cbf-4f33-9344-c0dbf83add5c
---

User directive (2026-05-23): "Most questions are answered by the
principle 'what would a senior TS expert who is uncompromising
demand?'. After you have done substantial work spawn a review subagent
to carefully scrutinise the work."

**Why:** The auto-port-eval predecessor showed that the harness must
itself be high-quality or it lets bad ports through (HANDOFF.md §1-7
are all about subtle harness defects). The library that comes out
the other end is only as good as the rig that grades it. A "good
enough" harness ships a "good enough" MPFR. Neither is acceptable
for the warmup project that the user wants to scale to FLINT.

**How to apply:**

When facing a design choice in the harness, prompts, ports, or
infrastructure, ask: *what would an uncompromising senior TS
expert demand here?*

Concrete defaults this licenses:

- **Strict TS, no `any`.** `unknown` + narrowing, never `any`. No
  `@ts-ignore`, no `as` casts without a structural justification in
  a comment. `tsconfig.json` strict + noUncheckedIndexedAccess +
  exactOptionalPropertyTypes + noPropertyAccessFromIndexSignature.
- **ESM modules only, no CommonJS.** `"type": "module"` in
  package.json. Imports use explicit extensions where the runtime
  requires it.
- **No `let` where `const` works.** Mutation is justified, not
  default.
- **Immutability by default** — `readonly` on every interface
  field where mutation isn't required. Already baked into
  src/core.ts (Law 4) but applies everywhere.
- **No silent failures, no try/catch swallowing.** Errors carry
  the failing input in their message. Rule 1 already says this for
  ports; it applies to the harness too.
- **Tests assert invariants, not "didn't throw".** Rule 7 is
  explicit but worth repeating in every subagent prompt.
- **Fast feedback over plausible code.** Subagents should run
  their own acceptance check before reporting done, never "this
  should work" without proof.
- **No comments narrating WHAT.** Comments explain WHY, cite a
  source (Law 1), or call out a non-obvious invariant. Otherwise
  the code speaks for itself.

**Review-subagent cadence:**

After each substantial step or batch of steps (rough rule: every
~300 LOC or every 3 pilot steps, whichever comes first), spawn a
review subagent with:
- The files changed in the batch
- The CLAUDE.md Laws + Rules
- This memory
- A specific brief: "scrutinise as an uncompromising senior TS
  expert; surface every defect that would block code review; no
  praise prose, just findings"

Review findings go into bd as `bug` issues if they require fixes,
or are addressed inline if trivial. Do not skip the review because
"the step passed its acceptance test" — acceptance tests pass
shallow ports too.
