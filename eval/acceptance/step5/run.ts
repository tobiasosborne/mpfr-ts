/**
 * run.ts — acceptance driver for the runner.ts contract (Step 5, RED).
 *
 * Drives the (yet-to-be-implemented) `bun eval/harness/runner.ts` across
 * five scenarios — correct / broken / infloop / schema-violator /
 * NaN-equality — and asserts each scenario produces the expected
 * grade.json shape. The scenarios collectively pin the contract Step 6
 * will implement:
 *
 *   (a) correct  — composite >= 0.95, no schema violation
 *   (b) broken   — composite < 0.5  (wrong ternary), no schema violation
 *   (c) infloop  — n_infloop == n_cases, wall_ms < 30000
 *   (d) violator — schema_violation=true, composite=0, n_cases=0,
 *                  schema_errors non-empty
 *   (e) nan      — composite >= 0.95, NaN-equality semantics
 *
 * RED-phase behaviour: runner.ts does not exist yet. Each `Bun.spawn`
 * exits non-zero with "Module not found", every scenario FAILs, and
 * this driver exits non-zero. That is the expected RED state — Step 6
 * will turn it GREEN.
 *
 * Ref: CLAUDE.md Rule 5 — "mutation-prove the goldens" / port-and-verify
 *   TDD shape. This driver IS the mutation prover for the runner: it
 *   bakes the failure modes into ports/*.ts and demands the runner
 *   surface them in grade.json.
 *
 * Type-surface note: tsconfig has `"types": []`, so we declare the
 * tiny ambient surface we use (Bun.spawn, the subset of process, and
 * the node:fs sync existsSync + readFileSync) directly here rather
 * than pulling in @types/bun / @types/node. The harness's other files
 * (value_codec.ts, ast_check.ts) avoid these dependencies for the same
 * reason; worker.ts uses an analogous local-declare pattern for the
 * webworker globals.
 */

// ---------------------------------------------------------------------------
// Ambient declarations — minimal subset we touch.
//
// Bun and Node both define these globals at runtime; the tsconfig's
// `types: []` keeps the project free of @types/* dependencies (zero
// runtime deps, per CLAUDE.md Rule 12, extends to type-only deps for
// hygiene). We re-declare only the methods this driver calls.
// ---------------------------------------------------------------------------

interface SpawnOptions {
  readonly cmd: readonly string[];
  readonly cwd?: string;
  readonly stdout?: 'pipe' | 'inherit' | 'ignore';
  readonly stderr?: 'pipe' | 'inherit' | 'ignore';
}

interface Subprocess {
  readonly stdout: ReadableStream<Uint8Array> | null;
  readonly stderr: ReadableStream<Uint8Array> | null;
  readonly exited: Promise<number>;
}

interface BunFile {
  exists(): Promise<boolean>;
  text(): Promise<string>;
  delete(): Promise<void>;
}

interface BunNamespace {
  spawn(opts: SpawnOptions): Subprocess;
  file(path: string): BunFile;
}

declare const Bun: BunNamespace;

declare const process: {
  exit(code?: number): never;
};

// ---------------------------------------------------------------------------
// Project paths. All absolute so Bun.spawn cwd doesn't matter; the runner
// reads relative imports from the port file's own directory anyway.
// ---------------------------------------------------------------------------

const REPO = '/home/tobias/Projects/mpfr-ts';
const RUNNER = `${REPO}/eval/harness/runner.ts`;
const STEP5 = `${REPO}/eval/acceptance/step5`;

// ---------------------------------------------------------------------------
// Grade-json shape. Subset of what runner.ts is contracted to write —
// we only assert the fields each scenario needs. `by_tag` is optional
// because the violator scenario short-circuits before any cases run, and
// we don't want optional-narrowing to gate the structural check.
// ---------------------------------------------------------------------------

interface GradeJson {
  readonly schema_violation: boolean;
  readonly schema_errors: readonly string[];
  readonly composite_correctness: number;
  readonly n_cases: number;
  readonly n_pass: number;
  readonly n_throw: number;
  readonly n_timegate: number;
  readonly n_infloop: number;
  readonly first_error: string | null;
  readonly wall_ms: number;
  readonly by_tag?: Readonly<Record<string, { readonly n: number; readonly n_pass: number }>>;
}

/**
 * Field-narrowing helpers. Each returns the value on success or an
 * error-description string on failure. The caller uses identity on the
 * tag union (the success types are `boolean | number | array | null`,
 * disjoint from the error type `string` for everything but the string
 * field — which we handle separately with a result-typed return).
 */
function fieldBool(r: Readonly<Record<string, unknown>>, k: string): boolean | { err: string } {
  const v = r[k];
  return typeof v === 'boolean' ? v : { err: `field '${k}': expected boolean, got ${typeof v}` };
}
function fieldNum(r: Readonly<Record<string, unknown>>, k: string): number | { err: string } {
  const v = r[k];
  return typeof v === 'number' ? v : { err: `field '${k}': expected number, got ${typeof v}` };
}
function fieldStrArr(
  r: Readonly<Record<string, unknown>>,
  k: string,
): readonly string[] | { err: string } {
  const v = r[k];
  if (!Array.isArray(v)) return { err: `field '${k}': expected array, got ${typeof v}` };
  for (let i = 0; i < v.length; i++) {
    if (typeof v[i] !== 'string') {
      return { err: `field '${k}[${i}]': expected string, got ${typeof v[i]}` };
    }
  }
  return v as readonly string[];
}
function fieldNullableStr(
  r: Readonly<Record<string, unknown>>,
  k: string,
): string | null | { err: string } {
  const v = r[k];
  if (v === null) return null;
  if (typeof v === 'string') return v;
  return { err: `field '${k}': expected string|null, got ${typeof v}` };
}

function isErr<T>(x: T | { err: string }): x is { err: string } {
  return typeof x === 'object' && x !== null && 'err' in x && typeof (x as { err: unknown }).err === 'string';
}

/**
 * Narrow an unknown JSON-decoded value to {@link GradeJson}. Returns
 * either a typed grade or a one-line shape-failure description.
 *
 * We check field-by-field rather than blanket-casting because runner.ts
 * is unwritten and a half-implemented version might emit a partial
 * blob — surfacing "missing field X" beats `undefined.foo` crashes
 * downstream.
 */
function asGradeJson(raw: unknown): GradeJson | { err: string } {
  if (raw === null || typeof raw !== 'object' || Array.isArray(raw)) {
    return { err: `expected object, got ${Array.isArray(raw) ? 'array' : typeof raw}` };
  }
  const r = raw as Readonly<Record<string, unknown>>;

  const schema_violation = fieldBool(r, 'schema_violation');
  if (isErr(schema_violation)) return schema_violation;
  const schema_errors = fieldStrArr(r, 'schema_errors');
  if (isErr(schema_errors)) return schema_errors;
  const composite_correctness = fieldNum(r, 'composite_correctness');
  if (isErr(composite_correctness)) return composite_correctness;
  const n_cases = fieldNum(r, 'n_cases');
  if (isErr(n_cases)) return n_cases;
  const n_pass = fieldNum(r, 'n_pass');
  if (isErr(n_pass)) return n_pass;
  const n_throw = fieldNum(r, 'n_throw');
  if (isErr(n_throw)) return n_throw;
  const n_timegate = fieldNum(r, 'n_timegate');
  if (isErr(n_timegate)) return n_timegate;
  const n_infloop = fieldNum(r, 'n_infloop');
  if (isErr(n_infloop)) return n_infloop;
  const first_error = fieldNullableStr(r, 'first_error');
  if (isErr(first_error)) return first_error;
  const wall_ms = fieldNum(r, 'wall_ms');
  if (isErr(wall_ms)) return wall_ms;

  let by_tag: GradeJson['by_tag'] = undefined;
  const rawBy = r['by_tag'];
  if (rawBy !== undefined) {
    if (rawBy === null || typeof rawBy !== 'object' || Array.isArray(rawBy)) {
      return { err: `field 'by_tag': expected object, got ${typeof rawBy}` };
    }
    const out: Record<string, { n: number; n_pass: number }> = {};
    for (const [k, v] of Object.entries(rawBy)) {
      if (v === null || typeof v !== 'object') {
        return { err: `field 'by_tag.${k}': expected object, got ${typeof v}` };
      }
      const b = v as Readonly<Record<string, unknown>>;
      const n = b['n'];
      const p = b['n_pass'];
      if (typeof n !== 'number' || typeof p !== 'number') {
        return { err: `field 'by_tag.${k}': expected {n: number, n_pass: number}` };
      }
      out[k] = { n, n_pass: p };
    }
    by_tag = out;
  }

  return {
    schema_violation,
    schema_errors,
    composite_correctness,
    n_cases,
    n_pass,
    n_throw,
    n_timegate,
    n_infloop,
    first_error,
    wall_ms,
    ...(by_tag !== undefined ? { by_tag } : {}),
  };
}

// ---------------------------------------------------------------------------
// Scenario definitions
// ---------------------------------------------------------------------------

interface SpawnMeta {
  readonly exitCode: number | null;
  readonly stdout: string;
  readonly stderr: string;
  /** Wallclock measured around the spawn, ms. */
  readonly elapsedMs: number;
}

interface Scenario {
  readonly name: string;
  readonly port: string;
  readonly golden: string;
  /**
   * Validator: returns null on PASS, or a one-line reason on FAIL.
   * Receives the parsed grade.json (or `null` if the file doesn't
   * exist) plus the spawn metadata.
   */
  readonly check: (grade: GradeJson | null, meta: SpawnMeta) => string | null;
}

const SCENARIOS: readonly Scenario[] = [
  {
    name: '(a) correct',
    port: `${STEP5}/ports/correct.ts`,
    golden: `${STEP5}/goldens/correct.jsonl`,
    check: (g) => {
      if (g === null) return 'no grade.json produced';
      if (g.schema_violation) return `schema_violation=true (unexpected)`;
      if (g.composite_correctness < 0.95) {
        return `composite=${g.composite_correctness} < 0.95`;
      }
      return null;
    },
  },
  {
    name: '(b) broken',
    port: `${STEP5}/ports/broken.ts`,
    golden: `${STEP5}/goldens/broken.jsonl`,
    check: (g) => {
      if (g === null) return 'no grade.json produced';
      if (g.schema_violation) return `schema_violation=true (unexpected)`;
      if (g.composite_correctness >= 0.5) {
        return `composite=${g.composite_correctness} >= 0.5 (wrong-ternary port should be < 0.5)`;
      }
      return null;
    },
  },
  {
    name: '(c) infloop',
    port: `${STEP5}/ports/infloop.ts`,
    golden: `${STEP5}/goldens/infloop.jsonl`,
    check: (g, meta) => {
      if (g === null) return 'no grade.json produced';
      if (g.schema_violation) return `schema_violation=true (unexpected)`;
      if (g.n_cases <= 0) return `n_cases=${g.n_cases} (expected > 0)`;
      if (g.n_infloop !== g.n_cases) {
        return `n_infloop=${g.n_infloop} != n_cases=${g.n_cases}`;
      }
      // Two wallclock budgets, both must hold: the run's self-report
      // (`wall_ms`) AND the externally-measured spawn time. The latter
      // catches a runner that mis-reports its own wallclock.
      if (g.wall_ms >= 30000) return `wall_ms=${g.wall_ms} >= 30000`;
      if (meta.elapsedMs >= 30000) {
        return `externally-measured spawn elapsedMs=${meta.elapsedMs.toFixed(0)} >= 30000`;
      }
      return null;
    },
  },
  {
    name: '(d) schema_violator',
    port: `${STEP5}/ports/schema_violator.ts`,
    golden: `${STEP5}/goldens/schema_violator.jsonl`,
    check: (g) => {
      if (g === null) return 'no grade.json produced';
      if (!g.schema_violation) return `schema_violation=false (expected true)`;
      if (g.composite_correctness !== 0) {
        return `composite=${g.composite_correctness} (expected 0 on schema violation)`;
      }
      if (g.n_cases !== 0) {
        return `n_cases=${g.n_cases} (expected 0; ast_check should reject pre-flight)`;
      }
      if (g.schema_errors.length === 0) {
        return `schema_errors empty (expected non-empty)`;
      }
      return null;
    },
  },
  {
    name: '(e) nan_equality',
    port: `${STEP5}/ports/nan_equality.ts`,
    golden: `${STEP5}/goldens/nan_equality.jsonl`,
    check: (g) => {
      if (g === null) return 'no grade.json produced';
      if (g.schema_violation) return `schema_violation=true (unexpected)`;
      if (g.composite_correctness < 0.95) {
        return `composite=${g.composite_correctness} < 0.95 (NaN-equality must hold)`;
      }
      return null;
    },
  },
];

// ---------------------------------------------------------------------------
// Spawn one scenario
// ---------------------------------------------------------------------------

async function readOptionalJson(path: string): Promise<unknown | null> {
  const file = Bun.file(path);
  if (!(await file.exists())) return null;
  try {
    const text = await file.text();
    return JSON.parse(text) as unknown;
  } catch (e) {
    console.error(`  [warn] read ${path} failed: ${e instanceof Error ? e.message : e}`);
    return null;
  }
}

async function drainStream(stream: ReadableStream<Uint8Array> | null): Promise<string> {
  if (stream === null) return '';
  const reader = stream.getReader();
  const decoder = new TextDecoder();
  let out = '';
  for (;;) {
    const { value, done } = await reader.read();
    if (done) break;
    if (value !== undefined) out += decoder.decode(value, { stream: true });
  }
  out += decoder.decode();
  return out;
}

interface ScenarioResult {
  readonly pass: boolean;
  readonly reason: string;
  readonly meta: SpawnMeta;
  readonly grade: GradeJson | null;
}

async function runScenario(s: Scenario): Promise<ScenarioResult> {
  // Distinct output path per scenario keeps the artifacts inspectable
  // post-run and prevents one scenario's grade.json from being read by
  // the next if the runner fails to write.
  const outputPath = `/tmp/step5_${s.name.replace(/[^a-z0-9]+/gi, '_')}.json`;
  // Clean stale output so a no-write run is distinguishable from a
  // previous-run leftover. Bun.file().delete() is a no-op-equivalent
  // on missing files in practice (it rejects, which we swallow).
  try {
    await Bun.file(outputPath).delete();
  } catch {
    /* file may not exist or unlink may race; benign — readOptionalJson re-checks */
  }

  const args: readonly string[] = [
    'bun',
    RUNNER,
    '--function',
    'acceptanceFn',
    '--port',
    s.port,
    '--golden',
    s.golden,
    '--output',
    outputPath,
    '--workers',
    '4',
    '--class',
    'arithmetic',
  ];

  const t0 = performance.now();
  const proc = Bun.spawn({
    cmd: args,
    cwd: REPO,
    stdout: 'pipe',
    stderr: 'pipe',
  });
  // Drain stdout/stderr concurrently with waiting for exit so a noisy
  // runner can't deadlock on a full pipe buffer.
  const [stdout, stderr, exitCode] = await Promise.all([
    drainStream(proc.stdout),
    drainStream(proc.stderr),
    proc.exited,
  ]);
  const elapsedMs = performance.now() - t0;

  const meta: SpawnMeta = { exitCode, stdout, stderr, elapsedMs };
  const rawGrade = await readOptionalJson(outputPath);

  let grade: GradeJson | null = null;
  if (rawGrade !== null) {
    const narrowed = asGradeJson(rawGrade);
    if (isErr(narrowed)) {
      return {
        pass: false,
        reason: `malformed grade.json: ${narrowed.err}`,
        meta,
        grade: null,
      };
    }
    grade = narrowed;
  }

  const reason = s.check(grade, meta);
  if (reason === null) {
    return { pass: true, reason: 'OK', meta, grade };
  }
  return { pass: false, reason, meta, grade };
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

async function main(): Promise<number> {
  console.log('Step-5 acceptance driver — runner.ts contract (RED-phase)');
  console.log(`  runner: ${RUNNER}`);
  if (!(await Bun.file(RUNNER).exists())) {
    console.log(`  note:   runner.ts does NOT exist yet — every scenario will FAIL`);
    console.log(`          this is the expected RED state for Step 5`);
  }
  console.log('');

  const results: Array<{ name: string; pass: boolean; reason: string; spawnExit: number | null }> = [];

  for (const s of SCENARIOS) {
    const { pass, reason, meta } = await runScenario(s);
    results.push({ name: s.name, pass, reason, spawnExit: meta.exitCode });
    const tag = pass ? 'PASS' : 'FAIL';
    console.log(`  ${tag}  ${s.name}: ${reason}`);
    if (!pass && meta.stderr.trim().length > 0) {
      // Surface the runner's stderr first non-empty line for diagnostic
      // context. We deliberately truncate to one line to keep the summary
      // compact; the full stderr is reachable by re-running the failing
      // scenario.
      const firstLine = meta.stderr.split('\n').find((l) => l.trim().length > 0);
      if (firstLine !== undefined) {
        console.log(`         stderr: ${firstLine.trim()}`);
      }
    }
  }

  console.log('');
  console.log('Summary');
  console.log('-------');
  for (const r of results) {
    const tag = r.pass ? 'PASS' : 'FAIL';
    console.log(`  ${tag}  ${r.name}${r.pass ? '' : `  — ${r.reason}`}`);
  }

  const failed = results.filter((r) => !r.pass).length;
  console.log('');
  console.log(`${results.length - failed}/${results.length} scenarios passed`);
  return failed === 0 ? 0 : 1;
}

const code = await main();
process.exit(code);

// This file is run as a script (top-level await), but tsconfig has
// `isolatedModules: true` which requires a module context to allow
// top-level await. The empty `export {}` makes this an ES module
// without exporting anything observable.
export {};
