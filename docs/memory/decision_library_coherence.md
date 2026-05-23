---
name: decision-library-coherence
description: "End-state is a usable composable TS library, not 600 isolated functions. Locked schema in src/core.ts; grader rejects ports that redeclare it; integration suite gates Production exit."
metadata: 
  node_type: memory
  type: project
  originSessionId: ae70f34d-7cbf-4f33-9344-c0dbf83add5c
---

**Decision:** Every port imports a locked schema from `src/core.ts` —
`MPFR` value type, `RoundingMode` string enum, `Result = {value,
ternary}` shape, `MPFRError` class. The schema is **versioned and
frozen**; changes require an ADR + a full library re-grade. Public
API is camelCase, strips `mpfr_` prefix (`setD`, not `mpfr_set_d`),
and preserves the C name in `@mpfrName` JSDoc.

**Why:** User said: "I really want is that the port is *usable*: at
the end every function should be usable together coherently with a
unified schema that is a logical TS version of the C orig." The
default risk is per-function ports each pass their golden but use
mutually-incompatible types — one uses `{kind, sign, exp, mant}`,
another uses `{tag, neg, e, m}`, a third encodes rounding mode as an
integer. The result is 600 isolated functions, not a library.

**How to apply:**

1. **Every prompt includes `src/core.ts` verbatim** with explicit
   instructions: "import these types, do not redeclare, return `Result`."
2. **Grader AST-checks every port** before running tests:
   - Must `import { MPFR, RoundingMode, Result } from "../core.ts"`
     (or appropriate relative path).
   - Must NOT contain `interface MPFR` or `type RoundingMode`
     redeclarations.
   - Composite=0 with `error="schema-violation"` on failure.
3. **Runner validates returned values structurally** — kind in
   the enum, sign in {1, -1}, prec is `bigint >= 1n`, mant is `bigint`,
   ternary in {-1, 0, 1}. Wrong shape is wrong even if numeric
   content matches.
4. **Integration test suite** in `eval/integration/<chain>.ts`
   exercises multi-function pipelines (`setD → mul → add → getD`)
   using only public `src/index.ts` exports. Production phase
   **cannot exit** until the integration suite passes 100%.
5. **Schema bumps go through an ADR**. Any port written against v1
   of the schema is invalidated when v2 ships and gets re-graded.
   Don't bump casually.

This is **Law 4** in CLAUDE.md ("The library composes") and is
non-negotiable. A port that passes its own golden but breaks
composition with already-shipped ports is a regression, not a
feature.
