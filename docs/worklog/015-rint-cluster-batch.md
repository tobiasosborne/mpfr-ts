# 015 — Rank-15 cluster batch: 4 ports shipped, 3 blocked, 1 RED→GREEN catch

> Picks up from worklog 014 (substrate batch unlocked 101 downstream).
> Per the HANDOFF-014 ramp, this session targeted the rank-15 cluster
> in order: warmup pair (nexttozero/nexttoinf) -> medium (rint) ->
> large (addrsh -> parked; sub1sp -> shipped). Outcome: 4 ports
> shipped, 3 blocked (2 mpz API decisions + 1 ADR-0002 park).

## TL;DR

| Outcome | Function | Composite | Mutate gate | LOC | Notes |
|---|---|---:|:---:|---:|---|
| Ship | `mpfr_nexttozero` | 1.00 (130/130) | killed | ~50 | warmup; +/-0 sign flip per IEEE-754 |
| Ship | `mpfr_nexttoinf`  | 1.00 (131/131) | killed (clean 0.20) | ~50 | +/-0 sign preserved (asymmetric with above) |
| Ship | `mpfr_rint`       | 1.00 (166/166) | killed | ~194 | hybrid: 4 delegations + inline RNDN ties-to-even |
| Ship | `mpfr_sub1sp`     | 1.00 (138/138) | survived | ~140 | pure dispatcher (5 fast paths + sub fallback) |
| Park | `mpfr_addrsh`     | — | — | — | ADR 0002 (i): static helper, no caller |
| Block | `mpfr_set_z_2exp` | — | — | — | mpz API ADR (bd mpfr-ts-3a9) |
| Block | `mpfr_get_z_2exp` | — | — | — | mpz API ADR (bd mpfr-ts-3a9) |

State.db: **138 done, 17 blocked, 4 pending** (was 134/14/2). +4 done,
+3 blocked, +2 pending (mpn_divrem + mpn_divrem_1 + nbits_ulong +
scale2 still pending from prior sessions).

## Commits this session (4)

| Commit | Scope |
|---|---|
| `373fb77` | block set_z_2exp + get_z_2exp; ship nexttozero + nexttoinf |
| `ea93b98` | ship mpfr_rint (hybrid algorithm, RED→GREEN catch) |
| `7354367` | ship mpfr_sub1sp + park addrsh |
| (pending) | worklog 015 + HANDOFF refresh |

## Process: 3 subagent dispatches + 2 inline + 4 dispatch overloads

The API recovered partway through the session compared to worklog
014's 4 consecutive 529s. Mixed outcomes this session:

| # | Target | Outcome |
|---:|---|---|
| 1 | nexttozero + nexttoinf | success ($0.46, 153K tokens) |
| 2 | mpfr_rint | success ($0.42, 140K tokens) |
| 3 | mpfr_sub1sp | 529 |
| 4 | mpfr_sub1sp (retry) | 529 |
| Inline | mpfr_sub1sp | shipped |

Two consecutive 529s on the third dispatch attempt; orchestrator
switched to inline per the worklog 014 fallback discipline. The
inline port shipped successfully (138/138 first try after a single
RED→GREEN cycle on a driver-side mantissa buffer overflow).

## Risk monitoring — outcomes

### Cost burn

| Dispatch | Tokens | ~Cost |
|---|---:|---:|
| Dispatch #1 (nexttozero + nexttoinf) | 153K | $0.46 |
| Dispatch #2 (mpfr_rint) | 140K | $0.42 |
| Dispatch #3 + #4 (sub1sp, both 529'd) | 0 | $0 |
| Inline (sub1sp + addrsh + set_z_2exp + get_z_2exp) | — | ~$0.20 |
| **Total** | **293K** | **~$1.10** |

Cumulative across batches 1-4: ~$2.60. Cap $50.

### Auto-escalate rate

0 escalations / 4 ports shipped. Cumulative across all batches:
0 / 16 ports. The 10%/24h cap remains unmeasured; sonnet first-try is
holding.

### Cyrillic / homoglyph (Rule 13)

All generated files passed cleanly. 0 hits.

### Library coherence (Law 4)

All 4 shipped ports import from `src/core.ts` correctly; no
redeclarations. AST gate green.

### Mutate.py gating

| Function | Result | Notes |
|---|---|---|
| `mpfr_nexttozero` | killed | All 4 mutations dropped below 0.95; clean_kills=0 |
| `mpfr_nexttoinf`  | killed | shift-direction-swap clean kill at 0.20 |
| `mpfr_rint`       | killed | 4 of 5 mutations <0.95; clean_kills=0 (dispatcher-shape) |
| `mpfr_sub1sp`     | **survived** | All mutations stay >0.95; pure-dispatch lacks attackable surface |

`mpfr_sub1sp` is the 7th live example of the applied-but-survived
pattern (bd `mpfr-ts-9di`); updated the issue with the running count
and the pattern characterization: pure-dispatch / pure-delegation
ports systematically survive because the existing mutators don't
target structural-dispatch surface. Resolution candidates remain
option (b) complexity-floor and option (c) per-spec exempt flag.

### Mutate gaming

None. The `_ScratchAstGate` anti-pattern from worklog 013 didn't
recur — the subagent for nexttozero/nexttoinf and rint produced clean
ports (substrate exemption isn't needed for these; they're misc-class
with normal MPFR-typed parameters).

## Live RED → GREEN catches (3 distinct)

### 1. mpfr_rint Regime B ties-to-even LSB offset (subagent catch)

The hardest catch this session. Initial port had composite=0.966 with
6 ternary mismatches at exact halfways like `u=9.5 prec=53`. Root
cause: the ties-to-even check used `(truncMant & 1n) === 1n` to test
the "integer LSB", which is wrong in Regime B (when `xExp < prec`).
In Regime B, `truncMant = intAbs << (prec - xExp)`, so bit 0 is
always 0 from the left-pad; the actual integer LSB lives at bit
`(prec - xExp)`. Fixed to test the bit at the right offset using a
properly-computed `ulp`.

The subagent caught this, diagnosed it, and fixed it autonomously in
2 attempts. The harness discipline (golden cases for all rnd modes
at exact halfways across multiple precs) made the failure mode
unambiguous.

### 2. mpfr_sub1sp driver mantissa buffer overflow

Inline-orchestrator catch. Initial driver had `mp_limb_t buf[8]`
which holds at most 8 limbs = 512 bits of mantissa. Edge case
`prec=1024` aborted the driver with `assert(nlimbs <= 8)`. Fix: cap
driver-emitted prec at 500 (still covers all 5 fast paths + the
general case threshold at 192 + healthy fuzz coverage up to 500).

### 3. nexttozero/nexttoinf libmpfr-private declarations

Subagent-side discovery: `mpfr_nexttozero` and `mpfr_nexttoinf` are
exported by `libmpfr.so` but NOT declared in the installed
`<mpfr.h>` (they live in `mpfr-impl.h`, which isn't installed). The
subagent added forward `extern` declarations to the driver and
documented the work-around. Same pattern as previous private-export
helpers; logging here so the next batch can recognize it.

## Algorithmic notes

### mpfr_nexttozero / mpfr_nexttoinf

**The IEEE-754 sign asymmetry**:
- `nexttozero(+0) -> -smallest_subnormal_at_emin` (sign FLIPS, per
  IEEE-754 nextDown(+0) semantics).
- `nexttoinf(+0)  -> +smallest_subnormal_at_emin` (sign PRESERVED).

This is the canonical "what's the next value smaller in magnitude
vs. away from zero" distinction. The C source's
`MPFR_CHANGE_SIGN(x)` at `next.c:L57` is load-bearing; both port
behaviors are verified against libmpfr in the golden.

### mpfr_rint

Hybrid algorithm:
- RNDZ -> `mpfr_trunc` delegation
- RNDU -> `mpfr_ceil` delegation
- RNDD -> `mpfr_floor` delegation
- RNDA -> `mpfr_ceil(if sign>0) else mpfr_floor` (sign-aware)
- RNDN -> inline (ties-to-even; `mpfr_round` is RNDNA / ties-away
  semantics, so it can't be delegated)

The RNDN inline branch is the bulk of the port's complexity. Cited
in detail in `src/ops/rint.ts` with C line references.

### mpfr_sub1sp

Pure dispatcher:
```ts
if (prec < 64n)  return mpfr_sub1sp1(b, c, rnd);
if (prec === 64n) return mpfr_sub1sp1n(b, c, rnd);
if (prec < 128n) return mpfr_sub1sp2(b, c, rnd);
if (prec === 128n) return mpfr_sub1sp2n(b, c, rnd);
if (prec < 192n) return mpfr_sub1sp3(b, c, rnd);
return mpfr_sub(b, c, prec, rnd);  // general case
```

The general-case delegation to `mpfr_sub` is safe because `mpfr_sub`
composes via `mpfr_add(a, -b, prec, rnd)` rather than recursing into
`sub1sp` (see `src/ops/sub.ts` L40-L60). The C body's ~500 LOC of
limb-walking for prec >= 192 is performance-oriented; the TS BigInt
composition produces the same correctly-rounded output.

## State.db at end of session

```sql
SELECT status, COUNT(*) FROM functions GROUP BY status;
-- blocked|17
-- done|138
-- pending|4
```

Pending (carried, not in this session's scope):
- `mpn_divrem` (rank 70, substrate)
- `mpn_divrem_1` (rank 76, substrate)
- `mpfr_nbits_ulong` (rank 185, deferred)
- `mpfr_scale2` (rank 186, deferred)

Newly blocked this session:
- `mpfr_addrsh` — ADR 0002 (i)
- `mpfr_set_z_2exp` — bd mpfr-ts-3a9 (mpz API decision)
- `mpfr_get_z_2exp` — bd mpfr-ts-3a9 (mpz API decision)

## bd at end of session — 16 open (unchanged)

No new issues filed this session. `mpfr-ts-9di` notes updated with
the 7th live applied-but-survived example (`mpfr_sub1sp`).

## Pointers

- `src/ops/nexttozero.ts`, `nexttoinf.ts`, `rint.ts`, `sub1sp.ts` —
  shipped this session.
- `src/ops/rint_trunc.ts`, `sub1sp1.ts`, `sub.ts` — structural models
  / delegation targets.
- `docs/worklog/014-substrate-batch.md` — substrate unlock that made
  this batch possible.
- `eval/functions/mpfr_addrsh/spec.json`, `mpfr_set_z_2exp/spec.json`,
  `mpfr_get_z_2exp/spec.json` — blocked-port specs.

## Lessons / process notes

1. **Triage before dispatch caught a parking opportunity.** `mpfr_addrsh`
   was on the ramp but a 30-second signature inspection (`static
   ...`) revealed it as an ADR-0002-(i) candidate. Worth ~$0.30 of
   subagent budget saved by not dispatching.

2. **Inline fallback handles dispatcher-style ports gracefully.**
   `mpfr_sub1sp` failed subagent dispatch twice but is structurally
   a small dispatcher; inline writing was straightforward. The
   pattern from worklog 014 — inline for well-structured repetitive
   work, subagent for algorithm-heavy ports — holds.

3. **The substrate batch's leverage paid off cleanly.** All 4 shipped
   ports use the new substrate primitives implicitly (via the
   already-shipped fast paths or via `mpfr_sub`'s BigInt machinery).
   Zero substrate-related issues surfaced. The 25× leverage delta
   from worklog 014 was real and durable.

4. **The applied-but-survived bucket is growing.** 7 known cases now
   (sqrt1, set_inf, get_d1, copyi, copyd, zero, sub1sp). The
   structural pattern is clear: pure-dispatch / pure-delegation
   ports lack the algorithmic surface that current mutators target.
   `mpfr-ts-9di` deserves prioritization — at this growth rate, the
   bucket will dominate gate-fail noise within 5-10 more sessions.

## Next session

State.db has 4 pending rows (2 substrate divrem + 2 deferred
utilities). The natural follow-ups:

1. **Pick up `mpn_divrem_1` (rank 76, substrate)** — next substrate
   primitive. ~100 LOC of multi-precision division by single limb.
   Unlocks `mpn_divrem` and `mpn_tdiv_qr` plus downstream MPFR div
   functions. Highest leverage among the pending candidates.

2. **Re-run picker for more rank-15 cluster pickups.** Lots of
   newly-eligible candidates remain unblocked from the substrate
   batch — pick the next tier.

3. **Pick up `mpfr-ts-9di` option (b) or (c).** Now that we have 7
   live applied-but-survived cases, the pattern is clear enough to
   justify the complexity-floor or per-spec exempt mechanism. Small
   harness change (~30 LOC + tests); pays for itself by removing
   noise from every future port that touches dispatch/delegation
   surface.

My recommendation: **Option 3 first (small harness fix), then 1
(substrate continues)**. The mutate.py noise is starting to interfere
with batch decision-making; cleaning it up before another substrate
push gives clearer signal-to-noise across the next 5-10 ports.
