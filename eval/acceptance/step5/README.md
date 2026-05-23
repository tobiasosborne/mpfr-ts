# Step-5 acceptance scaffold — runner.ts contract

This directory is the RED-phase acceptance suite for the harness's
`runner.ts`. It defines what the runner must do, in executable form,
**before runner.ts exists**. Step 6 implements the runner; running
this driver before Step 6 produces the expected RED failure for every
scenario.

## What the runner is contracted to deliver

```
bun eval/harness/runner.ts \
  --function <fnName> \
  --port <path-to-port.ts> \
  --golden <path-to-golden.jsonl> \
  --output <path-to-grade.json> \
  [--class <substrate|arithmetic|transcendental|misc>] \
  [--workers <N>]
```

The runner writes a `grade.json` of shape:

```ts
interface GradeJson {
  schema_violation: boolean;
  schema_errors: string[];
  composite_correctness: number;        // 0..1
  n_cases: number;
  n_pass: number;
  n_throw: number;
  n_timegate: number;
  n_infloop: number;
  first_error: string | null;
  wall_ms: number;
  by_tag: Record<string, { n: number; n_pass: number }>;
}
```

On `schema_violation: true`, the runner does NOT spawn any worker;
`composite=0`, `n_cases=0`, and `schema_errors` is populated by
`eval/harness/ast_check.ts`.

## The five scenarios

| #   | Port                       | Golden                       | Expected grade.json                                                   |
| --- | -------------------------- | ---------------------------- | --------------------------------------------------------------------- |
| (a) | `ports/correct.ts`         | `goldens/correct.jsonl`      | `composite >= 0.95`, `schema_violation = false`                       |
| (b) | `ports/broken.ts`          | `goldens/broken.jsonl`       | `composite < 0.5`, `schema_violation = false`                         |
| (c) | `ports/infloop.ts`         | `goldens/infloop.jsonl`      | `n_infloop == n_cases`, `wall_ms < 30000`, `schema_violation = false` |
| (d) | `ports/schema_violator.ts` | `goldens/schema_violator.jsonl` | `schema_violation = true`, `composite = 0`, `n_cases = 0`, `schema_errors.length > 0` |
| (e) | `ports/nan_equality.ts`    | `goldens/nan_equality.jsonl` | `composite >= 0.95`, `schema_violation = false`                       |

Scenarios (a) and (e) share the same identity-on-input function body;
the distinction is the golden's input distribution (normal MPFR vs
NaN), which exercises the `compareMpfr` NaN short-circuit in
`value_codec.ts`.

## Files

```
step5/
├── README.md                 — this file
├── spec.json                 — function signature for the runner
├── run.ts                    — the acceptance driver (Bun)
├── ports/
│   ├── correct.ts            — (a) reference identity port
│   ├── broken.ts             — (b) wrong ternary (1 instead of 0)
│   ├── infloop.ts            — (c) sync while(true) {}
│   ├── schema_violator.ts    — (d) redeclares MPFR / Result; no core import
│   └── nan_equality.ts       — (e) same as correct.ts; NaN golden
└── goldens/
    ├── _gen.ts               — deterministic regenerator
    ├── correct.jsonl         — 10 normal-MPFR cases (5 tag classes)
    ├── broken.jsonl          — same 10 cases as correct.jsonl
    ├── infloop.jsonl         — 5 cases
    ├── schema_violator.jsonl — 5 cases (content irrelevant)
    └── nan_equality.jsonl    — 5 NaN-input cases
```

## How to run

```bash
# Regenerate the goldens (deterministic; only re-run after editing _gen.ts):
bun eval/acceptance/step5/goldens/_gen.ts

# Run the acceptance driver:
bun eval/acceptance/step5/run.ts
```

Today (RED): every scenario FAILs with "Module not found" or equivalent
because `eval/harness/runner.ts` does not exist. The driver exits
non-zero. **This is correct for Step 5.**

After Step 6 implements `runner.ts`: every scenario PASSes and the
driver exits zero.

## Authoring notes

- The goldens are generated, not hand-written, so a future change to
  the wire format (e.g. an additional field) can be propagated by
  editing `goldens/_gen.ts` and re-running. Diffing the regenerated
  files against the committed copy is the change-control surface.
- `ports/schema_violator.ts` intentionally type-checks under
  `tsc --noEmit` — the violation is at the **library-coherence** layer
  (Law 4), not the TypeScript layer. ast_check.ts is what catches it.
- `ports/infloop.ts` exploits TS control-flow narrowing on
  `while (true) {}` to satisfy `Result`-return type without a
  `// @ts-expect-error` or `as never` cast. The body never returns;
  TS infers `never` for the function and the declared return type
  is trivially compatible.
- `run.ts` reads the spawn's stdout and stderr concurrently to avoid
  pipe-buffer deadlock. Each scenario writes to a distinct
  `/tmp/step5_<name>.json` so post-run inspection is easy.
