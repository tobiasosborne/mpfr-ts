---
name: mpfr-storage-traps
description: "Three load-bearing facts about MPFR's value storage that bit the Step 4 author and will bite every future driver / port author who doesn't know them."
metadata: 
  node_type: memory
  type: project
  originSessionId: ae70f34d-7cbf-4f33-9344-c0dbf83add5c
---

Established during Step 4 (golden-driver common.h, mpfr-ts-odi.4). All three are
empirically verified, not assumed.

**1. `mpfr_get_z_2exp` already de-pads to `prec`-bit MSB-aligned.**
The function calls `mpn_rshift` internally to strip the `limb_bits *
ceil(prec/limb_bits) - prec` trailing zero bits before returning. So the
returned `|z|` is already exactly the MSB-aligned mantissa the
[[decision-library-coherence]] schema wants. Do NOT try to read raw
`_mpfr_d` limbs and shift yourself — the math is fiddly, the field
isn't public API, and you'll be reinventing what `mpfr_get_z_2exp`
already does correctly.

Ref: `mpfr/src/get_z_2exp.c` L73-75.

**2. TS-schema `exp` ≠ `mpfr_get_z_2exp`'s returned exponent.**
`mpfr_get_z_2exp` returns `exp_2` such that `value = sign(z) * |z| *
2^exp_2`. The src/core.ts convention is `value = sign * mant * 2^(exp -
prec)`. Therefore: `exp_ts = exp_2 + prec`, which equals
`MPFR_GET_EXP(f)`. Off-by-`prec` is the easy bug. Pattern in C:

```c
mpfr_exp_t exp_2 = mpfr_get_z_2exp(z, x);
int64_t exp_ts = (int64_t) exp_2 + (int64_t) mpfr_get_prec(x);
// emit "exp": exp_ts
```

**3. NaN sentinel diverges from C side.**
MPFR's C side retains the originating precision on NaN (so `mpfr_set_d(NAN, RNDN)` at prec=53 gives a NaN with `MPFR_PREC = 53`). The src/core.ts convention is `prec === 0n, sign === 1` for NaN — see [[decision-library-coherence]]. The value codec must NOT propagate `MPFR_PREC(x)` for NaN inputs/outputs; emit `"prec":"0","sign":1` literally, or `validate()` will reject and the grader will spurious-fail every NaN case.

This divergence is also why round-tripping NaN through libmpfr won't preserve precision — that's intentional, NaN-propagating ops have no well-defined output precision in the idiomatic-TS surface.

**How to apply:**
- Driver authors (Step 6+): use the `jl_kv_mpfr` helper in `common.h` — it
  handles all three correctly. Don't roll your own.
- Codec author (Step 5): when decoding NaN, ignore the wire `prec` and
  produce `NAN_VALUE` from core.ts directly.
- Port authors (sonnet L3 in the ralph loop): the prompt template
  (Step 9) must call out the NaN-prec convention explicitly. Sonnet
  will hallucinate "preserve input prec on NaN" otherwise — it's the
  obvious C behaviour.
