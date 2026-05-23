---
name: decision-api-shape
description: "TypeScript API is idiomatic immutable. Each MPFR op returns {value, ternary}, no mutation. Faithful destructive C-mirror layer is NOT being built."
metadata: 
  node_type: memory
  type: project
  originSessionId: ae70f34d-7cbf-4f33-9344-c0dbf83add5c
---

**Decision:** All ported MPFR functions expose an idiomatic immutable TypeScript signature:

```ts
export function add(a: MPFR, b: MPFR, prec: bigint, rnd: RoundingMode): { value: MPFR; ternary: number };
```

Not the faithful C contract (`mpfr_add(rop, a, b, rnd) → ternary; rop mutated`).

**Why:** User explicitly said "pure idiomatic TypeScript" from the first message. Asked the trade-off explicitly with three options (idiomatic / faithful / both) — chose idiomatic. Faithful would be 1:1 diffable with C but yield an awkward C-style TS API that nobody would use.

**How to apply:**
- Golden master harness needs per-function adapters that translate C's destructive contract into idiomatic call/return for grading. (Inputs serialized → parsed → call idiomatic fn → compare returned `{value, ternary}` to golden.)
- The faithful internal substrate ([[decision-substrate]]) lives in src/internal/ and is NOT part of the public API.
- When porting prompts mention "the C contract is `mpfr_add(rop, a, b, rnd)`", always restate the TS signature as `add(a, b, prec, rnd) → {value, ternary}`.
- Special values: NaN, ±Inf, ±0 must be preserved in the `value` field. Signed zero matters.
