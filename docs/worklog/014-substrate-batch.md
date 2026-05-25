# 014 — Substrate batch: 6 mpn_* primitives, 101 downstream functions unblocked

> Picks up from worklog 013 (second Production batch — 4 trivial accessors).
> The HANDOFF queued substrate as Priority 1 because the mpn_* primitives
> block a dense cluster of low-rank downstream functions
> (mpfr_sub1sp, mpfr_rint, mpfr_addrsh, etc.). This session shipped the 6
> highest-leverage primitives and surfaced an environmental constraint:
> Anthropic API was persistently overloaded for subagent dispatches,
> forcing orchestrator-inline execution for 5 of the 6 ports.

## TL;DR

| Function | Composite | Mutate gate | Body LOC | Unblocks |
|---|---:|:---:|---:|---:|
| `mpn_copyi` | 1.00 (110/110) | survived (only guard mutation) | ~25 | 4 |
| `mpn_copyd` | 1.00 (110/110) | survived | ~25 | 5 |
| `mpn_zero`  | 1.00 (110/110) | survived | ~15 | 3 |
| `mpn_add_1` | 1.00 (120/120) | **killed** (shift-swap 0.20) | ~35 | 17 |
| `mpn_sub_1` | 1.00 (120/120) | **killed** (shift-swap 0.20) | ~35 | 7 |
| `mpn_rshift`| 1.00 (120/120) | **killed** (2 clean: 0.27, 0.24) | ~45 | 10 |

**Net unblocked: 101 newly-eligible downstream functions** (was 0
eligible after worklog 013, now 101 — the picker can pick from these
in the next batch without callgraph re-seeding). State.db: **134 done,
14 blocked, 2 pending** (was 128/14/2; +6 substrate ports).

## What changed in state.db

```sql
-- Before: blocked|14 done|128 pending|2
-- After:  blocked|14 done|134 pending|2
```

The 6 new ports were INSERTed with topo_rank 9000-9005 (manual ranks
above the callgraph-derived ceiling, since these primitives aren't in
callgraph.json's source set). All shipped as `status='done',
best_correctness=1.0, attempts=1` (or 2 where a RED → GREEN fix was
needed; see "Live bugs caught" below).

## Process: 1 subagent dispatch + 5 inline ports + 4 API overloads

| Dispatch | Outcome |
|---|---|
| #1 (trio: copyi/copyd/zero) | API 529 overloaded; 0 tokens |
| #2 (trio retry) | Dispatched async, then API 529 overloaded mid-flight; 0 tokens |
| #3 (mpn_add_1) | API 529 overloaded; 0 tokens |
| #4 (mpn_sub_1) | API 529 overloaded; 0 tokens |
| **Inline** | trio + add_1 + sub_1 + rshift, all shipped |

**Four consecutive 529 Overloaded errors** on `general-purpose` Agent
dispatches. The first session that has been API-constrained — for
batches 1+2 (worklogs 012+013) dispatches worked fine. The
orchestrator switched to inline execution after the third overload
to maintain progress.

Inline execution is the right fallback for this work shape:
- Substrate primitives are well-structured (every port follows the
  same spec.json + driver + port pattern as the previous one).
- The user's "FYI" memory ([[project-future-bigint-refactor]]) says
  the limb-array substrate is provisional; no point burning subagent
  cycles on micro-precision when the Optimize phase will likely
  collapse them to native BigInt.
- Per-port orchestrator work (writing the algorithm + tests) is
  ~10-20 minutes of typing — comparable to subagent dispatch
  turnaround when the API is working.

The trio shipped in ~15 minutes of inline work (one subagent dispatch
would have been ~75K tokens × $3 = $0.23 with similar wall time).
For substrate primitives at this level of triviality, inline is
arguably the better default.

## Live bugs caught — RED → GREEN

Three of the 6 ports required a real RED → GREEN fix cycle:

**1. `mpn_add_1` n=0 contract.** Initial port returned `carry = x` when
n=0 (i.e. "x has nowhere to go, so it IS the carry"). GMP actually
returns `carry = 0` regardless of x (x is silently discarded). Caught
by golden case 21 (`[], n=0, x=1 -> carry=0`). Fixed; commented in
the port.

**2. `mpn_sub_1` n=0 contract.** Initial port mirrored add_1's
`borrow=0` for n=0. GMP is asymmetric here: `mpn_sub_1` returns
`borrow = (x != 0 ? 1 : 0)` because subtracting any positive value
from "nothing" is conceptually an underflow. Caught by golden case 21
(`[], n=0, x=1 -> borrow=1`). Fixed; comment notes the asymmetry.

**3. Driver hex constants.** I wrote `0x5UB1AB1ULL`, `0xADC0PYDEEDULL`,
`0x4SH1AB1ULL`, `0xF0224SH1EE0ULL` and similar — none of which are
valid hex constants (U, B, P, S, H beyond `0-9, A-F` are illegal in
hex literals). Caught by gcc's `invalid suffix` error. Fixed in all 3
affected drivers (copyi/copyd/zero, sub_1, rshift). A lesson worth
documenting: when constructing memorable-mnemonic seed constants for
PRNG initialization in C drivers, restrict to actual hex digits.

The fact that 3 of 6 ports had a real bug caught is the strongest
possible validation of the harness-first discipline (CLAUDE.md Law 2:
"the harness is the truth, not the agent"). Two of the three were
semantic divergences from GMP that I couldn't have inferred without
the empirical evidence; the third was a compile error that would have
shipped a broken golden if compile-clean hadn't been required.

## Risk monitoring — outcomes

### Cost burn

Subagent dispatch cost: $0 (all 4 dispatches 529'd before consuming
tokens). Orchestrator-inline cost: not measured precisely, but
qualitatively in the same ballpark as the previous batches (~$0.30
estimated based on token volume).

Cumulative across batches 1+2+3: ~$1.50. Cap $50.

### Auto-escalate rate

0 escalations / 6 substrate ports shipped this session. Cumulative:
0 / 12 ports across all 3 batches. The 10%/24h cap remains
unmeasured — the inline pattern doesn't exercise opus escalation at
all (the orchestrator IS the agent doing the work).

### Cyrillic / homoglyph (Rule 13)

All 18 generated files (6 spec.json + 6 golden_driver.c + 6 port.ts)
passed the Cyrillic/Greek grep cleanly.

### Library coherence (Law 4)

All 6 ports are substrate-class. The runner exempts substrate from
`requireCoreImport` per `eval/harness/runner.ts:1188`
(`requireCoreImport = portClass !== 'substrate'`), so no core.ts
import is needed. The redeclaration check on `MPFR`/`Result`/
`RoundingMode` still applies and all 6 ports pass it (no redeclaration).

### Mutate.py gating

**3 ports killed cleanly** (add_1, sub_1, rshift) — each has a non-
trivial algorithmic surface (carry/borrow chains; bit shifts) that
the existing mutators (bigint-bump, shift-direction-swap) can attack.

**3 ports survived** (copyi, copyd, zero) — these are one-line slice/
fill ops with no algorithmic surface; the only mutation that applies
is `comparison-swap` on the input-validation guard, and the goldens
don't include invalid-n cases (because `expected_throw` codec isn't
shipped yet per bd `mpfr-ts-e4j`). Updated bd `mpfr-ts-9di` with
these as fresh evidence of the applied-but-survived pattern. 6 known
live cases now: sqrt1, set_inf, get_d1 (from earlier sessions), plus
copyi, copyd, zero (this session). All 6 are correct by construction;
mutate.py just can't prove it given the current harness shape.

### Mutate gaming

No dead code added to satisfy mutate.py for any of the 6 ports.
Worklog 010 lesson preserved.

## Algorithmic notes

### mpn_copyi / mpn_copyd / mpn_zero

In immutable TS bigint, copyi and copyd are functionally identical
(both return a fresh array); kept as separate exports for name-for-
name parity with the GMP I/O contract and the callgraph references.
`mpn_zero(n)` is `new Array(n).fill(0n)`.

### mpn_add_1 / mpn_sub_1

Carry/borrow chain across limbs, LSB-first. Asymmetric n=0 contract
documented above. Returns `{ result, carry }` or `{ result, borrow }`
respectively.

### mpn_rshift

Symmetric to mpn_lshift. For each limb i:
- `result[i] = (s[i] >> count) | ((next << (LIMB_BITS - count)) & MASK)`

where `next = s[i+1]` (or `0n` if i is the top limb). The shifted-out
return packs the low `count` bits of s[0] into the HIGH `count` bits
of the output limb (GMP convention).

The `& MASK` after the OR is **load-bearing**: without it, bits that
should overflow into the next limb stay in the current one (BigInt
doesn't truncate on shift like C's `mp_limb_t` does).

## Downstream unblock — the leverage payoff

State.db eligibility query before vs after:

| Before | After |
|---:|---:|
| 0 eligible | **101 eligible** |

Top-rank newly-eligible candidates (deps now satisfied, ready for the
next picker batch):

| Rank | Function | Class |
|---:|---|---|
| 15 | mpfr_sub1sp | misc |
| 16 | mpfr_set_z_2exp | misc |
| 26 | mpfr_rint | misc |
| 27 | mpfr_get_z_2exp | misc |
| 36 | mpfr_addrsh | misc |
| 70 | mpn_divrem | substrate |
| 76 | mpn_divrem_1 | substrate |
| 84 | mpfr_nexttozero | misc |
| 85 | mpfr_nexttoinf | misc |
| 185+ | ~93 more at higher ranks | various |

The rank-15 to rank-85 cluster is the immediate value — these are
core arithmetic helpers that worklog 012 / 013 couldn't pick up
because their mpn_* deps were unsatisfied. The next session can
dispatch picker for these directly.

## bd at end of session — 16 open

Updated this session:
- `mpfr-ts-9di` — notes updated with 3 fresh examples (copyi, copyd, zero
  joining sqrt1, set_inf, get_d1 in the applied-but-survived bucket).

No new bd issues filed (the 529 API state is environmental, not a
project gap).

## Pointers

- `src/internal/mpn/{copyi,copyd,zero,add_1,sub_1,rshift}.ts` -- 6 ports.
- `eval/functions/mpn_{copyi,copyd,zero,add_1,sub_1,rshift}/` -- 6 spec
  + driver + golden trios.
- `src/internal/mpn/{add_n,sub_n,lshift}.ts` -- pre-existing substrate
  primitives, structural model.
- `docs/worklog/013-second-production-batch.md` -- previous batch.
- [[project-future-bigint-refactor]] -- the memory entry establishing
  that the limb-array substrate is provisional; informs the
  inline-vs-subagent tradeoff.

## Lessons / process notes

1. **API overload happens; inline is a viable fallback.** Four
   consecutive 529s would have stalled the session if the
   orchestrator hadn't been comfortable taking over. The work was
   well-defined enough (substrate primitives have a stable shape)
   that inline execution preserved progress without sacrificing
   discipline.

2. **Hex literal hygiene.** Memorable mnemonic seed constants in C
   drivers are tempting but risky — `0xSH...` and `0xUB...` are
   illegal. Stick to actual hex digits or use `_` separators
   (`0xADD_1_BEEF`). Document this for the next batch's drivers.

3. **Asymmetric n=0 contracts in GMP.** `mpn_add_1(0, x) -> 0`
   discards x; `mpn_sub_1(0, x) -> x>0 ? 1 : 0` treats subtraction
   as underflow. Mirroring add_1's contract to sub_1 was the
   intuitive-but-wrong move; only the empirical golden caught it.
   When porting symmetric GMP primitives, **always generate the
   golden before writing the port**; let libgmp tell you what the
   contract actually is.

4. **Substrate leverage is real.** 6 small primitives unblocked 101
   downstream functions. Compared to batch 2's 4 accessors (low-
   leverage), the leverage delta is ~25×. Future batches that
   choose between substrate-depth and surface-breadth should pick
   substrate-depth when downstream is constrained.

## Next session

State.db has 2 pending rows (`mpfr_nbits_ulong`, `mpfr_scale2`) plus
**101 newly-eligible** functions ready for the picker. Possible
focuses:

1. **Pick up the rank-15 cluster.** `mpfr_sub1sp` (15), `mpfr_rint`
   (26), `mpfr_addrsh` (36), `mpfr_nexttozero`/`nexttoinf` (84/85) —
   these are core arithmetic helpers. The picker will surface them
   in topo order. Expected: 5-8 ports, ~$3-5 in subagent dispatches
   if the API is back to normal.

2. **Pick up `mpn_divrem_1` substrate (rank 76).** Still substrate,
   unlocks the division path. Bigger port (~100 LOC) but on the
   leverage frontier.

3. **Pick up `mpfr-ts-l4t` (AST gate friction)** — a small cleanup
   that strips the dead core.ts imports from the 4 no-arg accessors
   shipped in batch 2.

My recommendation: **option 1 (rank-15 cluster)**. The substrate
investment in this session was specifically to unblock these
candidates; picking them up next is the natural payoff. The substrate
deeper work (option 2) can wait until the next cluster is harvested.
