---
name: decision-substrate
description: "Port GMP mpn_* and MPFR internal helpers as faithful TS substrate FIRST, then build idiomatic public fns on top."
metadata: 
  node_type: memory
  type: project
  originSessionId: ae70f34d-7cbf-4f33-9344-c0dbf83add5c
---

**Decision:** Build a faithful TypeScript substrate that mirrors GMP's mpn_* limb routines and MPFR's internal helpers (mpfr_round_raw_generic, mpfr_check_range, exponent handling, etc.) before porting any public function.

**Why:** User chose "Faithful substrate" explicitly over "Idiomatic BigInt-native" and over "Hybrid". The reasoning that won: every divergence from C output is debuggable line-by-line when internals match. If we used BigInt-native substrate, a wrong ternary flag in `mpfr_add` could come from rounding, normalization, OR limb arithmetic — three places to look. Faithful substrate localizes blame.

**How to apply:**
- The ralph loop's FIRST wave ports substrate functions (mpn_add_n, mpn_sub_n, mpn_mul_basecase, mpn_lshift, mpn_rshift, mpn_cmp, ...; then mpfr_round_raw_generic, mpfr_check_range, mpfr_set_exp_t, ...). These have faithful C-mirror signatures.
- Public functions ([[decision-api-shape]]) sit on top and are idiomatic.
- Layout: `src/internal/mpn/`, `src/internal/mpfr/` (faithful) vs `src/` (idiomatic public).
- Even though the substrate is faithful, it's still pure TS BigInt under the hood — "faithful" means matching the *algorithm* and *I/O contract*, not using SIMD intrinsics that JS lacks.
- Root-to-leaves ordering of the substrate is critical: mpn_add_n before mpn_sqr before mpfr_round_raw_generic before mpfr_add.
