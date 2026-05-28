# Worklog 023 — Mega batch: 16 clean-frontier ports (13 shipped, 3 port-blocked)

## Context

User requested a "megamega batch" of 50 functions in parallel waves of
10-20 via DeepSeek-Flash, with several sonnet preppers. First question
was feasibility against the callgraph.

## Frontier analysis (the headline finding)

A clean 50-port Flash batch is **not reachable**. Wave simulation against
`eval/driver/callgraph.json` (deps satisfied = status in {done,slow}):

- **Genuinely clean, no design work: 16 functions**, all one wave (no
  internal cascade among them).
- +7 to ~23 = printf/PRNG family (`sprintf`, `v*printf`,
  `random_deviate_*`) — look dep-free but need the printf + PRNG ADRs.
- Per-ADR unlock is shallow (prng→31, printf→26, mpq→24; prng+printf+mpq
  together only ~35).
- The jump to the 141 ceiling is gated behind the **blocked
  transcendental-aux cluster** (`atan_aux`, `cos2_aux`, `exp2_aux`,
  `exp_rational`, `sqrt2_approx`, `gamma_*_exact`, `cbrt`, `rem1`) — the
  HARD, approximation-graded ports, not mechanical Flash work.

User chose the clean batch. Saved to memory: `project_callgraph_frontier`.

## What changed

16 functions prepped via **4 parallel sonnet preppers** (partitioned by
difficulty), all goldens calibrated at composite=1.0 and mutation-proven
by the preppers:

- **6 flag setters** (exceptions.c, OR-set mirror of the shipped `clear_*`
  family): set_divby0, set_erangeflag, set_inexflag, set_nanflag,
  set_overflow, set_underflow.
- **4 config/range setters**: set_emax, set_emin, set_default_prec,
  set_default_rounding_mode.
- **3 value/predicate**: set_uj, total_order_p, set_prec_raw.
- **3 rounding**: roundeven (idiomatic), round_near_x + round_raw_generic
  (substrate → src/internal/mpfr/).

**13 shipped** (Flash-ported, graded composite=1.0, mutation-gated):
the 6 flag setters, set_emin, set_default_prec, set_default_rounding_mode,
set_uj, set_prec_raw, roundeven, round_raw_generic. Flash PORT cost
~$0.07 total (~$0.005/fn).

**3 port-blocked** (prep COMPLETE — spec + driver + golden calibrated at
1.0 + reference port all committed; only the Flash PORT step is
outstanding): set_emax, total_order_p, round_near_x. See Frictions.

## Why these choices

- 4 preppers (not 1) per the user's "several sonnet preppers" and because
  the 16 split into 4 distinct shapes. Flag setters batched under one
  prepper (clear_* gives a 1:1 template); rounding core got its own
  prepper (hardest — round_raw_generic is the core mpn-limb routine).
- Flash PORT ran parallel-2 (HANDOFF caution on mpfr-ts-75v cold-start
  variance), not parallel-8.
- round_raw_generic shipped to src/internal/mpfr/round_raw_generic.ts
  (NOT round_raw.ts — that's the existing single-bigint roundMantissa;
  the new one is the faithful limb-array form). Named to match the
  callgraph node, avoiding the collision.

## Frictions surfaced

1. **opencode cold-start hangs (mpfr-ts-75v) bit hard.** set_emax,
   total_order_p, round_near_x each hung at the full timeout (300s then
   420s) with EMPTY logs — opencode never streamed a byte. Not a prep
   problem (goldens calibrate at 1.0, reference ports exist). set_emin
   (near-identical to set_emax) ported fine, and round_near_x's opencode
   process WAS streaming healthily when killed at wind-up — so it's
   intermittent runtime variance, not prompt content. These 3 just need
   a Flash PORT retry when opencode is calm; everything else is ready.
2. **Substrate class mismatch: DB row vs spec.json.** `ralph.py --grade`
   reads class from state.db (seeded from callgraph = "misc"), NOT from
   spec.json. round_raw_generic is substrate; the misc class triggered
   the core-import AST gate and failed grading. Fixed by
   `UPDATE functions SET class='substrate'` for round_raw_generic +
   round_near_x. **General lesson: when a prepper reclassifies a fn as
   substrate, the DB row must be updated too, or --grade/--ship mis-route.**
3. **Non-ASCII in Flash output not covered by the normalize.**
   round_raw_generic port had `∈` (U+2208) and `±` (U+00B1) in comments;
   the run_deepseek_port safe-Unicode normalize only covers arrows.
   Normalized by hand (∈→in, ±→+/-). Candidate to add to the normalize set.
4. **`--grade` flag takes positional fns** (`--grade FN [FN...]`), not
   `--fn` — minor CLI gotcha.

## Acceptance

- 13 ports: composite=1.0 against full-coverage goldens (Rule 7 minimums
  met: happy>=20, edge>=30, adversarial>=10, fuzz>=50, mined>=5).
- All 13 pass the mutation gate (7 killed, 6 vacuous — the vacuous ones
  are the trivial single-bit flag setters; goldens independently
  mutation-proven by preppers via reference-port bit-flips, so signal is
  solid and there's no mutator-bait).
- state.db: 232 done (was 219), 11 pending, 24 blocked.

## Pointers

- Memory: `project_callgraph_frontier` (wave analysis + ADR-gating map).
- 3 port-blocked fns: prep under eval/functions/mpfr_{set_emax,
  total_order_p,round_near_x}/; reference ports under
  eval/reference_ports/correct/. Retry: `python3
  eval/driver/run_deepseek_port.py --fn <name>` then `ralph.py --grade
  <name> --model deepseek-anthropic/deepseek-v4-flash --effort L3`.
- A prepper flagged a harness insight: boundary mutants confined to the
  0.2-weight `edge` class can survive the composite gate; discriminating
  cases for subtle bugs should land in the corr-weighted classes
  (happy/fuzz/adversarial). Worth a future harness ticket.
