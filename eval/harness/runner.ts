/**
 * runner.ts — the Pilot-phase grader for mpfr-ts.
 *
 * Drives a port through a JSONL golden master, isolates every case in a
 * fresh Bun `Worker` slot, and writes a `grade.json` describing the run.
 * This is THE load-bearing component CLAUDE.md Rule 4 calls out: each
 * test case runs with a parent-side hard-wall timeout because a ported
 * transcendental can synchronously infinite-loop, and only
 * `worker.terminate()` from the parent can recover from that — a
 * worker-internal AbortController cannot interrupt a sync busy-loop
 * because the event loop never yields.
 *
 * Contract (CLI flags, grade.json shape, composite formula) is pinned by
 * eval/acceptance/step5/run.ts — that driver is the executable spec.
 *
 * High-level flow:
 *
 *   1. Parse CLI args; locate spec.json next to the port or the golden.
 *   2. Pre-flight: astCheck() against the port source. On failure, write
 *      a schema-violation grade.json (composite=0, n_cases=0) and exit 0.
 *      No worker is ever spawned.
 *   3. Load the golden JSONL (one record per line; malformed lines are
 *      classified as n_throw rather than aborting the run).
 *   4. Spawn a pool of N workers; each init's against the port URL with
 *      a 2-second handshake timeout (catches top-level await / infinite
 *      work in the module body).
 *   5. Dispatch cases to idle workers. Each case races the worker's
 *      reply against a class-tier per-case budget (50/200/1000 ms). On
 *      timeout, terminate that worker, respawn a fresh one, and mark
 *      the case n_infloop.
 *   6. Classify each result: pass / throw / timegate (completed but over
 *      budget) / infloop (terminated). Bucket by tag.
 *   7. Composite = 0.6*corr + 0.2*edge + 0.2*mined, with 0/0 → 1.0 per
 *      tag class (vacuously perfect).
 *   8. Write grade.json (pretty-printed, 2-space indent).
 *
 * Ref: CLAUDE.md Laws 2, 4; Rules 4, 5, 7, 12.
 * Ref: eval/harness/worker.ts — child side of the protocol.
 * Ref: eval/harness/value_codec.ts — input/output decoding and comparison.
 * Ref: eval/harness/ast_check.ts — schema-violation gate.
 * Ref: eval/acceptance/step5/run.ts — executable contract.
 */

import {
  decodeInputValue,
  decodeExpectedOutput,
  compareOutput,
  type ExpectedOutput,
} from './value_codec.ts';
import { astCheck, type AstCheckResult } from './ast_check.ts';

// ---------------------------------------------------------------------------
// Ambient declarations — minimal subset we touch. tsconfig has `types: []`
// so we re-declare the Bun / Node globals locally rather than pull in
// `@types/bun` / `@types/node` (CLAUDE.md Rule 12: zero runtime deps,
// extended to type-only deps for hygiene).
// ---------------------------------------------------------------------------

interface BunFile {
  exists(): Promise<boolean>;
  text(): Promise<string>;
}

interface BunNamespace {
  file(path: string): BunFile;
  write(path: string, data: string): Promise<number>;
}

declare const Bun: BunNamespace;

declare const process: {
  readonly argv: readonly string[];
  exit(code?: number): never;
};

declare const console: {
  log(...args: readonly unknown[]): void;
  error(...args: readonly unknown[]): void;
};

declare const performance: { now(): number };

declare const setTimeout: (cb: () => void, ms: number) => number;
declare const clearTimeout: (handle: number) => void;

// import.meta.main is Bun's "is this the entry script?" flag. The
// `Node22+ --experimental-strip-types` path doesn't provide it, but
// the runner is only invoked via `bun eval/harness/runner.ts` per the
// CLI contract — the published src/ stays runtime-agnostic. Per Rule
// 12 the harness is Bun-only; ports work on Bun OR Node.
interface ImportMeta {
  readonly main?: boolean;
  readonly url: string;
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/** Class-tier per-case hard-wall budgets, milliseconds. CLAUDE.md Rule 4. */
const CLASS_BUDGET_MS: Readonly<Record<PortClass, number>> = {
  substrate: 50,
  arithmetic: 50,
  transcendental: 200,
  misc: 1000,
};

/** Worker-init handshake timeout, milliseconds. */
const INIT_TIMEOUT_MS = 2000;

/** Default worker-pool size. */
const DEFAULT_WORKERS = 4;

/** Resolved next to runner.ts so Bun's Worker constructor finds it. */
const WORKER_URL = new URL('./worker.ts', import.meta.url);

/**
 * Valid tag values per CLAUDE.md Rule 7. We accept any string at runtime
 * (so an unknown tag from a malformed golden doesn't crash the run) but
 * the composite formula only buckets these five.
 */
const KNOWN_TAGS: ReadonlySet<string> = new Set([
  'happy',
  'edge',
  'adversarial',
  'fuzz',
  'mined',
]);

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export type PortClass = 'substrate' | 'arithmetic' | 'transcendental' | 'misc';

/** `spec.json` shape — sibling of port or golden. */
interface SpecJson {
  readonly function: string;
  readonly class: PortClass;
  readonly signature: {
    readonly params: readonly string[];
    readonly returns?: string;
  };
}

/** Parsed CLI invocation. */
interface CliArgs {
  readonly fnName: string;
  readonly portPath: string;
  readonly goldenPath: string;
  readonly outputPath: string;
  readonly classOverride: PortClass | null;
  readonly workers: number;
  readonly specOverride: string | null;
}

/** One golden record as decoded from a JSONL line. */
interface GoldenCase {
  readonly idx: number;
  readonly tag: string;
  readonly inputs: Readonly<Record<string, unknown>>;
  readonly rawOutput: unknown;
}

/** Per-case classification accumulator. */
interface CaseOutcome {
  readonly idx: number;
  readonly tag: string;
  readonly passed: boolean;
  readonly threw: boolean;
  readonly timedOut: boolean; // worker.terminate() was called
  readonly timeGate: boolean; // completed but exceeded budget
  readonly error: string | null;
  readonly ms: number;
}

/** Final on-disk shape. Field order matches the contract; pretty-printed. */
export interface GradeJson {
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
  readonly by_tag: Readonly<Record<string, { readonly n: number; readonly n_pass: number }>>;
  readonly function: string;
  readonly class: PortClass;
  readonly port_path: string;
  readonly golden_path: string;
}

// ---------------------------------------------------------------------------
// Worker protocol — mirrors eval/harness/worker.ts. Duplicated by design
// (per the note in worker.ts: cross-file change shows up as a compile
// error on the next typecheck).
// ---------------------------------------------------------------------------

interface InitMessage {
  readonly type: 'init';
  readonly portUrl: string;
  readonly functionName: string;
}
interface CaseMessage {
  readonly type: 'case';
  readonly caseIdx: number;
  readonly inputArgs: readonly unknown[];
}
type ParentMessage = InitMessage | CaseMessage;

interface ReadyMessage {
  readonly type: 'ready';
}
interface InitErrorMessage {
  readonly type: 'init_error';
  readonly error: string;
}
interface ResultMessage {
  readonly type: 'result';
  readonly caseIdx: number;
  readonly ms: number;
  readonly ok: boolean;
  readonly value?: unknown;
  readonly error?: string;
}
type ChildMessage = ReadyMessage | InitErrorMessage | ResultMessage;

/**
 * Bun's `Worker` class. The lib `webworker` types describe this, but the
 * project's `lib: ["esnext"]` excludes the worker types deliberately —
 * runner.ts is the only place that constructs one, and a local declare
 * keeps the type surface tight.
 */
interface WorkerLike {
  postMessage(message: ParentMessage): void;
  terminate(): void;
  onmessage: ((evt: { data: ChildMessage }) => void) | null;
  onerror: ((err: unknown) => void) | null;
}

declare class Worker implements WorkerLike {
  constructor(url: URL | string, opts?: { type?: 'module' });
  postMessage(message: ParentMessage): void;
  terminate(): void;
  onmessage: ((evt: { data: ChildMessage }) => void) | null;
  onerror: ((err: unknown) => void) | null;
}

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------

/**
 * Compute composite_correctness from per-tag pass rates.
 *
 *   corr   = pass-rate over {happy, fuzz, adversarial} — vacuously 1.0
 *   edge   = pass-rate over {edge}                     — vacuously 1.0
 *   mined  = pass-rate over {mined}                    — vacuously 1.0
 *
 *   composite = 0.6 * corr + 0.2 * edge + 0.2 * mined
 *
 * "Vacuously 1.0" means a class with zero cases contributes its full
 * weight as if it had passed perfectly. This is the right behaviour for
 * goldens that legitimately lack one of the classes (a substrate port
 * with no `mined` cases shouldn't be penalised), and matches CLAUDE.md
 * Rule 7's framing.
 */
export function computeComposite(
  outcomes: readonly CaseOutcome[],
): number {
  let corrN = 0;
  let corrPass = 0;
  let edgeN = 0;
  let edgePass = 0;
  let minedN = 0;
  let minedPass = 0;
  for (const o of outcomes) {
    if (o.tag === 'edge') {
      edgeN++;
      if (o.passed) edgePass++;
    } else if (o.tag === 'mined') {
      minedN++;
      if (o.passed) minedPass++;
    } else if (
      o.tag === 'happy' ||
      o.tag === 'fuzz' ||
      o.tag === 'adversarial'
    ) {
      corrN++;
      if (o.passed) corrPass++;
    }
    // Unknown tags do not contribute. They still count toward n_cases /
    // n_pass / by_tag for diagnostics, but they don't move composite.
  }
  const corr = corrN === 0 ? 1.0 : corrPass / corrN;
  const edge = edgeN === 0 ? 1.0 : edgePass / edgeN;
  const mined = minedN === 0 ? 1.0 : minedPass / minedN;
  return 0.6 * corr + 0.2 * edge + 0.2 * mined;
}

/**
 * Bucket per-tag pass counts for the `by_tag` field. Includes every tag
 * the golden carried, even unknown ones, for diagnostic transparency.
 */
function bucketByTag(
  outcomes: readonly CaseOutcome[],
): Record<string, { n: number; n_pass: number }> {
  const out: Record<string, { n: number; n_pass: number }> = {};
  for (const o of outcomes) {
    const bucket = out[o.tag] ?? { n: 0, n_pass: 0 };
    bucket.n++;
    if (o.passed) bucket.n_pass++;
    out[o.tag] = bucket;
  }
  return out;
}

/**
 * JSON.stringify that emits BigInts as decimal strings. The grade.json
 * itself never contains BigInts today, but the safer replacer means
 * adding a diagnostic field that does later is fine.
 */
function jsonStringifyPretty(v: unknown): string {
  return JSON.stringify(
    v,
    (_k, val: unknown) => (typeof val === 'bigint' ? val.toString() : val),
    2,
  );
}

// ---------------------------------------------------------------------------
// CLI parsing
// ---------------------------------------------------------------------------

/**
 * Parse `--flag value` style argv. Unknown flags are an internal error
 * (exit non-zero). Missing required flags also internal-error so the
 * orchestrator can distinguish "wrote a grade" from "couldn't even start".
 */
function parseCli(argv: readonly string[]): CliArgs {
  // Drop the runtime path and script path (Bun mirrors Node here).
  const args = argv.slice(2);
  let fnName: string | null = null;
  let portPath: string | null = null;
  let goldenPath: string | null = null;
  let outputPath: string | null = null;
  let classOverride: PortClass | null = null;
  let workers = DEFAULT_WORKERS;
  let specOverride: string | null = null;

  for (let i = 0; i < args.length; i++) {
    const flag = args[i];
    const val = args[i + 1];
    if (val === undefined) {
      throw new Error(`flag ${flag} requires a value`);
    }
    switch (flag) {
      case '--function':
        fnName = val;
        break;
      case '--port':
        portPath = val;
        break;
      case '--golden':
        goldenPath = val;
        break;
      case '--output':
        outputPath = val;
        break;
      case '--class':
        classOverride = parsePortClass(val);
        break;
      case '--workers': {
        const n = Number.parseInt(val, 10);
        if (!Number.isFinite(n) || n < 1) {
          throw new Error(`--workers must be a positive integer, got ${val}`);
        }
        workers = n;
        break;
      }
      case '--spec':
        specOverride = val;
        break;
      default:
        throw new Error(`unknown flag: ${flag}`);
    }
    i++; // consumed flag's value
  }

  if (fnName === null) throw new Error('missing --function');
  if (portPath === null) throw new Error('missing --port');
  if (goldenPath === null) throw new Error('missing --golden');
  if (outputPath === null) throw new Error('missing --output');

  return {
    fnName,
    portPath,
    goldenPath,
    outputPath,
    classOverride,
    workers,
    specOverride,
  };
}

function parsePortClass(s: string): PortClass {
  if (s === 'substrate' || s === 'arithmetic' || s === 'transcendental' || s === 'misc') {
    return s;
  }
  throw new Error(
    `--class must be substrate|arithmetic|transcendental|misc, got '${s}'`,
  );
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------

/** Path manipulation kept tiny — no `node:path` import (Rule 12). */
function dirname(path: string): string {
  const idx = path.lastIndexOf('/');
  return idx === -1 ? '.' : path.substring(0, idx);
}

/**
 * Locate spec.json. Search order: --spec, sibling of port, sibling of
 * golden. Returns null if none of those exist; the caller falls back to
 * defaults (class='arithmetic', single positional input named after the
 * function's first golden record's keys).
 */
async function loadSpec(args: CliArgs): Promise<SpecJson | null> {
  const candidates: string[] = [];
  if (args.specOverride !== null) candidates.push(args.specOverride);
  candidates.push(`${dirname(args.portPath)}/spec.json`);
  candidates.push(`${dirname(args.goldenPath)}/spec.json`);
  for (const candidate of candidates) {
    const f = Bun.file(candidate);
    if (await f.exists()) {
      const text = await f.text();
      const parsed: unknown = JSON.parse(text);
      return narrowSpec(parsed, candidate);
    }
  }
  return null;
}

/**
 * Runtime-narrow a JSON-decoded `unknown` to {@link SpecJson}. We accept
 * the minimum surface used by the runner; extra fields are tolerated so
 * spec files can carry pilot-tracker metadata without breaking the gate.
 */
function narrowSpec(raw: unknown, path: string): SpecJson {
  if (raw === null || typeof raw !== 'object' || Array.isArray(raw)) {
    throw new Error(`spec.json at ${path}: expected object`);
  }
  const r = raw as Readonly<Record<string, unknown>>;
  const fn = r['function'];
  const cls = r['class'];
  const sig = r['signature'];
  if (typeof fn !== 'string') throw new Error(`spec.json at ${path}: 'function' must be string`);
  if (typeof cls !== 'string') throw new Error(`spec.json at ${path}: 'class' must be string`);
  const portClass = parsePortClass(cls);
  if (sig === null || typeof sig !== 'object' || Array.isArray(sig)) {
    throw new Error(`spec.json at ${path}: 'signature' must be object`);
  }
  const sigR = sig as Readonly<Record<string, unknown>>;
  const params = sigR['params'];
  if (!Array.isArray(params)) {
    throw new Error(`spec.json at ${path}: 'signature.params' must be array`);
  }
  const paramNames: string[] = [];
  for (let i = 0; i < params.length; i++) {
    const p = params[i];
    if (typeof p !== 'string') {
      throw new Error(`spec.json at ${path}: 'signature.params[${i}]' must be string`);
    }
    paramNames.push(p);
  }
  const returns = sigR['returns'];
  return {
    function: fn,
    class: portClass,
    signature: {
      params: paramNames,
      ...(typeof returns === 'string' ? { returns } : {}),
    },
  };
}

/**
 * Parse the JSONL golden. Blank lines are skipped silently; lines that
 * fail to parse are turned into a "malformed golden" outcome rather than
 * aborting the run — one corrupt line shouldn't suppress useful signal
 * from the other 99.
 */
function parseGolden(
  text: string,
  params: readonly string[],
): {
  readonly cases: readonly GoldenCase[];
  readonly malformed: ReadonlyArray<{ readonly idx: number; readonly error: string }>;
} {
  const cases: GoldenCase[] = [];
  const malformed: Array<{ idx: number; error: string }> = [];
  const lines = text.split('\n');
  let caseIdx = 0;
  for (let lineNo = 0; lineNo < lines.length; lineNo++) {
    const line = lines[lineNo];
    if (line === undefined) continue;
    if (line.trim().length === 0) continue;
    let parsed: unknown;
    try {
      parsed = JSON.parse(line);
    } catch (e) {
      malformed.push({
        idx: caseIdx,
        error: `malformed golden line ${lineNo + 1}: ${e instanceof Error ? e.message : String(e)}`,
      });
      caseIdx++;
      continue;
    }
    if (parsed === null || typeof parsed !== 'object' || Array.isArray(parsed)) {
      malformed.push({
        idx: caseIdx,
        error: `malformed golden line ${lineNo + 1}: expected object`,
      });
      caseIdx++;
      continue;
    }
    const r = parsed as Readonly<Record<string, unknown>>;
    const tag = r['tag'];
    const inputs = r['inputs'];
    if (typeof tag !== 'string') {
      malformed.push({
        idx: caseIdx,
        error: `malformed golden line ${lineNo + 1}: 'tag' missing or non-string`,
      });
      caseIdx++;
      continue;
    }
    if (inputs === null || typeof inputs !== 'object' || Array.isArray(inputs)) {
      malformed.push({
        idx: caseIdx,
        error: `malformed golden line ${lineNo + 1}: 'inputs' missing or non-object`,
      });
      caseIdx++;
      continue;
    }
    const _params = params; // satisfies noUnusedParameters chain
    void _params;
    cases.push({
      idx: caseIdx,
      tag,
      inputs: inputs as Readonly<Record<string, unknown>>,
      rawOutput: r['output'],
    });
    caseIdx++;
  }
  return { cases, malformed };
}

// ---------------------------------------------------------------------------
// Worker management
// ---------------------------------------------------------------------------

/**
 * Spawn one worker and complete its init handshake. Resolves to the
 * worker on success; rejects on init failure or 2s handshake timeout.
 * The caller owns the worker after resolve and must terminate it.
 */
async function spawnWorker(
  portUrl: string,
  functionName: string,
): Promise<Worker> {
  const w = new Worker(WORKER_URL, { type: 'module' });
  return new Promise<Worker>((resolve, reject) => {
    let settled = false;
    const handle = setTimeout(() => {
      if (settled) return;
      settled = true;
      w.terminate();
      reject(
        new Error(
          `worker init exceeded ${INIT_TIMEOUT_MS}ms (top-level work in module init?)`,
        ),
      );
    }, INIT_TIMEOUT_MS);
    w.onmessage = (evt) => {
      const m = evt.data;
      if (settled) return;
      if (m.type === 'ready') {
        settled = true;
        clearTimeout(handle);
        w.onmessage = null;
        w.onerror = null;
        resolve(w);
      } else if (m.type === 'init_error') {
        settled = true;
        clearTimeout(handle);
        w.terminate();
        reject(new Error(`worker init error: ${m.error}`));
      }
      // result messages can't arrive before ready; ignore defensively.
    };
    w.onerror = (err) => {
      if (settled) return;
      settled = true;
      clearTimeout(handle);
      w.terminate();
      reject(new Error(`worker onerror: ${String(err)}`));
    };
    w.postMessage({ type: 'init', portUrl, functionName });
  });
}

/**
 * Send one case to the worker and await its reply, with a parent-side
 * hard-wall timeout. On timeout: terminate, return `{ kind: 'timeout' }`;
 * the caller respawns. On reply: return `{ kind: 'result', msg }`.
 * `kind: 'crash'` covers worker `onerror` (uncaught exception in the
 * worker module itself) — treat as a per-case throw with the error
 * message, but the worker is then unusable and the caller should respawn.
 */
type CaseDispatchResult =
  | { readonly kind: 'result'; readonly msg: ResultMessage }
  | { readonly kind: 'timeout' }
  | { readonly kind: 'crash'; readonly error: string };

function dispatchCase(
  w: Worker,
  msg: CaseMessage,
  budgetMs: number,
): Promise<CaseDispatchResult> {
  return new Promise<CaseDispatchResult>((resolve) => {
    let settled = false;
    const handle = setTimeout(() => {
      if (settled) return;
      settled = true;
      w.onmessage = null;
      w.onerror = null;
      w.terminate();
      resolve({ kind: 'timeout' });
    }, budgetMs);
    w.onmessage = (evt) => {
      const m = evt.data;
      if (settled) return;
      if (m.type === 'result' && m.caseIdx === msg.caseIdx) {
        settled = true;
        clearTimeout(handle);
        w.onmessage = null;
        w.onerror = null;
        resolve({ kind: 'result', msg: m });
      }
      // Other shapes (ready, init_error, or stale results for another
      // caseIdx) are ignored; the timer remains armed.
    };
    w.onerror = (err) => {
      if (settled) return;
      settled = true;
      clearTimeout(handle);
      w.onmessage = null;
      w.onerror = null;
      w.terminate();
      resolve({ kind: 'crash', error: `worker onerror: ${String(err)}` });
    };
    w.postMessage(msg);
  });
}

// ---------------------------------------------------------------------------
// Main grading loop
// ---------------------------------------------------------------------------

/**
 * Decode a single case's inputs into the positional arg list the worker
 * will spread into the port. Throws on a missing param key — surfaces as
 * an n_throw outcome at the caller.
 */
function buildInputArgs(
  inputs: Readonly<Record<string, unknown>>,
  params: readonly string[],
): readonly unknown[] {
  const args: unknown[] = [];
  for (const k of params) {
    if (!(k in inputs)) {
      throw new Error(`golden inputs missing required param '${k}'`);
    }
    args.push(decodeInputValue(inputs[k]));
  }
  return args;
}

/**
 * Decode the case's expected output. Throws on a malformed wire record;
 * the caller turns that into an n_throw outcome.
 */
function decodeExpected(rawOutput: unknown): ExpectedOutput {
  return decodeExpectedOutput(rawOutput);
}

interface GradeContext {
  readonly args: CliArgs;
  readonly portClass: PortClass;
  readonly fnName: string;
  readonly params: readonly string[];
}

/**
 * Run all cases through a worker pool. Workers respawn after timeouts /
 * crashes so a single bad case doesn't poison the rest of the run.
 *
 * The dispatch loop is sequential per worker (each worker handles one
 * case at a time), but `workers` workers run in parallel. We don't try
 * to be clever about job stealing — Bun's Promise.race semantics + the
 * sequential per-worker pattern is enough for the Pilot.
 */
async function gradeCases(
  ctx: GradeContext,
  cases: readonly GoldenCase[],
  malformed: ReadonlyArray<{ readonly idx: number; readonly error: string }>,
): Promise<{
  outcomes: readonly CaseOutcome[];
  initFailed: boolean;
  initError: string | null;
}> {
  const budgetMs = CLASS_BUDGET_MS[ctx.portClass];
  const portUrl = pathToUrl(ctx.args.portPath);

  // Pre-build per-case outcomes for malformed lines; they need no worker.
  const malformedOutcomes: CaseOutcome[] = malformed.map((m) => ({
    idx: m.idx,
    tag: '__malformed__',
    passed: false,
    threw: true,
    timedOut: false,
    timeGate: false,
    error: m.error,
    ms: 0,
  }));

  if (cases.length === 0) {
    return { outcomes: malformedOutcomes, initFailed: false, initError: null };
  }

  // Initial pool. If even one worker can't init, the port is broken at a
  // level no per-case grade can capture — treat as a port-init failure
  // and return enough info for main() to write a "every case infloop"
  // grade.json with a clear first_error.
  const poolSize = Math.min(ctx.args.workers, cases.length);
  let workers: Worker[];
  try {
    workers = await Promise.all(
      Array.from({ length: poolSize }, () => spawnWorker(portUrl, ctx.fnName)),
    );
  } catch (e) {
    return {
      outcomes: malformedOutcomes,
      initFailed: true,
      initError: e instanceof Error ? e.message : String(e),
    };
  }

  // Job queue: shared index, each idle worker grabs the next case.
  // Sequential index access from async tasks is race-free under Bun's
  // single-threaded event loop in the parent (workers don't share JS
  // state with us; postMessage is the only channel).
  let nextCase = 0;
  const outcomes: CaseOutcome[] = new Array<CaseOutcome>(cases.length);

  // Per-worker driver. Loops grabbing cases until the queue empties,
  // respawning the worker after timeouts/crashes. Each driver owns at
  // most one Worker at a time; on resolve the driver's slot is empty.
  //
  // The local `worker` binding is reassigned across `await` boundaries
  // (respawn after timeout/crash), so TS can't narrow `Worker | null`
  // across awaits. We capture a non-null `live` reference at the top of
  // each iteration after the null-guard, and assign back to `worker`
  // when we drop/replace it.
  async function drive(slot: number): Promise<void> {
    let worker: Worker | null = workers[slot] ?? null;
    if (worker === null) return;
    for (;;) {
      // Capture the current worker into a non-null local at the top of
      // each iteration. The invariant maintained across iterations is
      // that `worker` is non-null on entry: every branch that sets it to
      // null either returns or immediately respawns into a non-null
      // Worker before falling through to the next iteration. The
      // assertion narrows TS's view (which is reset across each await).
      if (worker === null) {
        // Defensive: should be unreachable given the invariant above.
        return;
      }
      const live: Worker = worker;
      const idx = nextCase++;
      if (idx >= cases.length) {
        live.terminate();
        worker = null;
        return;
      }
      const c = cases[idx];
      if (c === undefined) {
        // Defensive: shouldn't happen given the bounds check above.
        live.terminate();
        worker = null;
        return;
      }
      let expected: ExpectedOutput;
      let inputArgs: readonly unknown[];
      try {
        expected = decodeExpected(c.rawOutput);
        inputArgs = buildInputArgs(c.inputs, ctx.params);
      } catch (e) {
        // Decode/build failure — record as a throw, keep using the same
        // worker for subsequent cases.
        outcomes[idx] = {
          idx: c.idx,
          tag: c.tag,
          passed: false,
          threw: true,
          timedOut: false,
          timeGate: false,
          error: `case ${c.idx}: ${e instanceof Error ? e.message : String(e)}`,
          ms: 0,
        };
        continue;
      }

      const dispatch = await dispatchCase(
        live,
        { type: 'case', caseIdx: c.idx, inputArgs },
        budgetMs,
      );

      if (dispatch.kind === 'timeout') {
        // Worker terminated; respawn for the next case.
        outcomes[idx] = {
          idx: c.idx,
          tag: c.tag,
          passed: false,
          threw: false,
          timedOut: true,
          timeGate: false,
          error: `case ${c.idx}: exceeded per-case budget ${budgetMs}ms (worker.terminate() fired)`,
          ms: budgetMs,
        };
        worker = null;
        // If more work remains, respawn. If respawn fails, we mark
        // subsequent cases as infloop with a synthetic error and break.
        if (nextCase < cases.length) {
          try {
            worker = await spawnWorker(portUrl, ctx.fnName);
          } catch (e) {
            const reason = e instanceof Error ? e.message : String(e);
            while (nextCase < cases.length) {
              const j = nextCase++;
              const cc = cases[j];
              if (cc === undefined) continue;
              outcomes[j] = {
                idx: cc.idx,
                tag: cc.tag,
                passed: false,
                threw: true,
                timedOut: false,
                timeGate: false,
                error: `respawn after timeout failed: ${reason}`,
                ms: 0,
              };
            }
            return;
          }
        }
        continue;
      }

      if (dispatch.kind === 'crash') {
        outcomes[idx] = {
          idx: c.idx,
          tag: c.tag,
          passed: false,
          threw: true,
          timedOut: false,
          timeGate: false,
          error: `case ${c.idx}: ${dispatch.error}`,
          ms: 0,
        };
        worker = null;
        if (nextCase < cases.length) {
          try {
            worker = await spawnWorker(portUrl, ctx.fnName);
          } catch (e) {
            const reason = e instanceof Error ? e.message : String(e);
            while (nextCase < cases.length) {
              const j = nextCase++;
              const cc = cases[j];
              if (cc === undefined) continue;
              outcomes[j] = {
                idx: cc.idx,
                tag: cc.tag,
                passed: false,
                threw: true,
                timedOut: false,
                timeGate: false,
                error: `respawn after crash failed: ${reason}`,
                ms: 0,
              };
            }
            return;
          }
        }
        continue;
      }

      // dispatch.kind === 'result'
      const msg = dispatch.msg;
      const overBudget = msg.ms > budgetMs;
      if (!msg.ok) {
        outcomes[idx] = {
          idx: c.idx,
          tag: c.tag,
          passed: false,
          threw: true,
          timedOut: false,
          timeGate: overBudget,
          error: `case ${c.idx}: port threw: ${msg.error ?? '(no error message)'}`,
          ms: msg.ms,
        };
        continue;
      }

      let cmp: string | null;
      try {
        cmp = compareOutput(msg.value, expected);
      } catch (e) {
        // compareOutput throws on internal-invariant violations (e.g.
        // an unhandled ExpectedOutput kind). Surface as n_throw with
        // a precise message rather than letting the runner exit non-zero.
        cmp = `compareOutput internal error: ${e instanceof Error ? e.message : String(e)}`;
      }

      if (cmp === null) {
        outcomes[idx] = {
          idx: c.idx,
          tag: c.tag,
          passed: true,
          threw: false,
          timedOut: false,
          timeGate: overBudget,
          error: null,
          ms: msg.ms,
        };
      } else {
        outcomes[idx] = {
          idx: c.idx,
          tag: c.tag,
          passed: false,
          threw: true,
          timedOut: false,
          timeGate: overBudget,
          error: `case ${c.idx}: ${cmp}`,
          ms: msg.ms,
        };
      }
    }
  }

  // Start one driver per pool slot. Each driver consumes from the shared
  // queue until exhausted. `Promise.all` waits for every driver to drain.
  await Promise.all(workers.map((_, i) => drive(i)));

  return {
    outcomes: [...malformedOutcomes, ...outcomes],
    initFailed: false,
    initError: null,
  };
}

/**
 * Convert a filesystem path to a URL string for `import()`. Bun accepts
 * `file://` URLs and bare paths; we use `file://` explicitly so the
 * worker's `await import(portUrl)` works regardless of cwd.
 */
function pathToUrl(p: string): string {
  if (p.startsWith('file://')) return p;
  if (p.startsWith('/')) return `file://${p}`;
  // Relative path: resolve against the runner's cwd via import.meta.url's
  // parent. Bun also supports bare relative paths, but file:// is clearer.
  const cwdUrl = new URL('./', import.meta.url);
  return new URL(p, cwdUrl).href;
}

// ---------------------------------------------------------------------------
// Schema-violation branch
// ---------------------------------------------------------------------------

/**
 * Build the grade.json for a schema-violation result. No workers were
 * spawned; n_cases=0, composite=0.
 */
function buildSchemaViolationGrade(
  ctx: GradeContext,
  ast: AstCheckResult,
  wallMs: number,
): GradeJson {
  const firstError =
    ast.errors.length > 0 ? `schema violation: ${ast.errors[0]}` : 'schema violation';
  return {
    schema_violation: true,
    schema_errors: ast.errors,
    composite_correctness: 0,
    n_cases: 0,
    n_pass: 0,
    n_throw: 0,
    n_timegate: 0,
    n_infloop: 0,
    first_error: firstError,
    wall_ms: wallMs,
    by_tag: {},
    function: ctx.fnName,
    class: ctx.portClass,
    port_path: ctx.args.portPath,
    golden_path: ctx.args.goldenPath,
  };
}

/**
 * Build the grade.json for a port-init failure. All cases are recorded
 * as n_infloop so the orchestrator sees a uniformly bad signal.
 */
function buildInitFailureGrade(
  ctx: GradeContext,
  nCases: number,
  initError: string,
  wallMs: number,
): GradeJson {
  return {
    schema_violation: false,
    schema_errors: [],
    composite_correctness: 0,
    n_cases: nCases,
    n_pass: 0,
    n_throw: 0,
    n_timegate: 0,
    n_infloop: nCases,
    first_error: `port init exceeded ${INIT_TIMEOUT_MS}ms (top-level work in module init?): ${initError}`,
    wall_ms: wallMs,
    by_tag: {},
    function: ctx.fnName,
    class: ctx.portClass,
    port_path: ctx.args.portPath,
    golden_path: ctx.args.goldenPath,
  };
}

// ---------------------------------------------------------------------------
// Final assembly
// ---------------------------------------------------------------------------

/**
 * Aggregate per-case outcomes into a GradeJson. Pure function — no I/O,
 * no clock. Wall time is supplied by the caller.
 */
function assembleGrade(
  ctx: GradeContext,
  outcomes: readonly CaseOutcome[],
  wallMs: number,
): GradeJson {
  // Strip the synthetic '__malformed__' outcomes for the by_tag bucket
  // (they aren't real cases) but keep them in counts so n_cases reflects
  // every line we tried to grade.
  const byTag = bucketByTag(outcomes.filter((o) => o.tag !== '__malformed__'));

  let nPass = 0;
  let nThrow = 0;
  let nTimeGate = 0;
  let nInfloop = 0;
  let firstError: string | null = null;
  for (const o of outcomes) {
    if (o.passed) {
      nPass++;
    } else if (o.timedOut) {
      nInfloop++;
    } else if (o.threw) {
      nThrow++;
    }
    if (o.timeGate && !o.timedOut) nTimeGate++;
    if (firstError === null && o.error !== null && !o.passed) {
      firstError = o.error;
    }
  }

  // Composite is computed against the real outcomes only (synthetic
  // malformed lines have an unknown tag and don't contribute to any
  // class — but their failure is reflected in n_throw + first_error).
  const composite = computeComposite(outcomes);

  return {
    schema_violation: false,
    schema_errors: [],
    composite_correctness: composite,
    n_cases: outcomes.length,
    n_pass: nPass,
    n_throw: nThrow,
    n_timegate: nTimeGate,
    n_infloop: nInfloop,
    first_error: firstError,
    wall_ms: wallMs,
    by_tag: byTag,
    function: ctx.fnName,
    class: ctx.portClass,
    port_path: ctx.args.portPath,
    golden_path: ctx.args.goldenPath,
  };
}

// ---------------------------------------------------------------------------
// Entrypoint
// ---------------------------------------------------------------------------

/**
 * Internal-error helper. Writes the message to stderr and exits non-zero.
 * Used only for the genuine "we couldn't start" cases (bad CLI args,
 * unreadable files); a grade-with-bad-composite is NOT an internal error
 * and exits 0.
 */
function die(msg: string): never {
  console.error(`runner.ts: ${msg}`);
  process.exit(2);
}

async function main(): Promise<number> {
  const t0 = performance.now();
  let args: CliArgs;
  try {
    args = parseCli(process.argv);
  } catch (e) {
    die(`bad CLI: ${e instanceof Error ? e.message : String(e)}`);
  }

  // Read port source and golden text up front. Failure here is an
  // internal error; the orchestrator can't proceed.
  const portFile = Bun.file(args.portPath);
  if (!(await portFile.exists())) {
    die(`port not found: ${args.portPath}`);
  }
  const portSource = await portFile.text();

  const goldenFile = Bun.file(args.goldenPath);
  if (!(await goldenFile.exists())) {
    die(`golden not found: ${args.goldenPath}`);
  }
  const goldenText = await goldenFile.text();

  // Load spec (optional; fall back to defaults).
  let spec: SpecJson | null = null;
  try {
    spec = await loadSpec(args);
  } catch (e) {
    die(`failed to load spec.json: ${e instanceof Error ? e.message : String(e)}`);
  }

  // Resolve class: --class wins over spec.json's class.
  const portClass: PortClass =
    args.classOverride ?? spec?.class ?? 'arithmetic';

  // Resolve params: spec.json wins; otherwise infer from the first golden
  // line's `inputs` object keys (insertion-order preserved by JSON.parse).
  // Inference is the fallback for spec-less ports; the inferred param
  // order is whatever the C driver emitted, which is the right contract.
  const params: readonly string[] = spec?.signature.params ?? inferParams(goldenText);

  const ctx: GradeContext = {
    args,
    portClass,
    fnName: args.fnName,
    params,
  };

  // --- Pre-flight: ast_check -------------------------------------------------
  // Substrate ports under src/internal/mpn/ legitimately deal in raw
  // bigint arrays, not MPFR values — they get the requireCoreImport=false
  // exemption per ast_check.ts's contract.
  const requireCoreImport = portClass !== 'substrate';
  const ast = astCheck(portSource, { requireCoreImport });
  if (!ast.ok) {
    const wallMs = performance.now() - t0;
    const grade = buildSchemaViolationGrade(ctx, ast, wallMs);
    await Bun.write(args.outputPath, jsonStringifyPretty(grade) + '\n');
    return 0;
  }

  // --- Parse golden ---------------------------------------------------------
  const { cases, malformed } = parseGolden(goldenText, params);

  if (cases.length === 0 && malformed.length === 0) {
    // Empty golden — produce a zero-cases grade and exit cleanly. This
    // is a strange but not internal-error state; the orchestrator can
    // observe `n_cases: 0` and decide.
    const wallMs = performance.now() - t0;
    const grade = assembleGrade(ctx, [], wallMs);
    await Bun.write(args.outputPath, jsonStringifyPretty(grade) + '\n');
    return 0;
  }

  // --- Grade ----------------------------------------------------------------
  const { outcomes, initFailed, initError } = await gradeCases(ctx, cases, malformed);

  if (initFailed) {
    // Worker pool couldn't start. Build the init-failure grade against
    // the full case count so n_infloop reflects every untested case.
    const wallMs = performance.now() - t0;
    const totalCases = cases.length + malformed.length;
    const grade = buildInitFailureGrade(
      ctx,
      totalCases,
      initError ?? 'unknown init failure',
      wallMs,
    );
    await Bun.write(args.outputPath, jsonStringifyPretty(grade) + '\n');
    return 0;
  }

  const wallMs = performance.now() - t0;
  const grade = assembleGrade(ctx, outcomes, wallMs);
  await Bun.write(args.outputPath, jsonStringifyPretty(grade) + '\n');

  // Compact stdout summary for human + orchestrator readability. The
  // canonical signal is grade.json; this is diagnostic only.
  const knownUnused = [...KNOWN_TAGS];
  void knownUnused;
  console.log(
    `[runner] function=${ctx.fnName} class=${portClass} ` +
      `n=${grade.n_cases} pass=${grade.n_pass} throw=${grade.n_throw} ` +
      `infloop=${grade.n_infloop} timegate=${grade.n_timegate} ` +
      `composite=${grade.composite_correctness.toFixed(4)} ` +
      `wall=${wallMs.toFixed(0)}ms`,
  );
  return 0;
}

/**
 * Infer positional param names from the first non-blank golden line's
 * `inputs` object. Used when no spec.json is found. Returns an empty
 * array if the golden is empty or malformed — gradeCases will then
 * surface that as per-case throws (golden inputs missing required param).
 */
function inferParams(goldenText: string): readonly string[] {
  for (const line of goldenText.split('\n')) {
    if (line.trim().length === 0) continue;
    let parsed: unknown;
    try {
      parsed = JSON.parse(line);
    } catch {
      continue;
    }
    if (parsed === null || typeof parsed !== 'object' || Array.isArray(parsed)) {
      continue;
    }
    const r = parsed as Readonly<Record<string, unknown>>;
    const inputs = r['inputs'];
    if (inputs === null || typeof inputs !== 'object' || Array.isArray(inputs)) {
      continue;
    }
    return Object.keys(inputs as Readonly<Record<string, unknown>>);
  }
  return [];
}

// ---------------------------------------------------------------------------
// Run only when this file is the script entrypoint.
// ---------------------------------------------------------------------------

if ((import.meta as ImportMeta).main === true) {
  try {
    const code = await main();
    process.exit(code);
  } catch (e) {
    // Top-level catch-all so an unhandled rejection still exits non-zero
    // with a useful message rather than Bun's default opaque trace.
    console.error(
      `runner.ts: unhandled error: ${e instanceof Error ? e.stack ?? e.message : String(e)}`,
    );
    process.exit(2);
  }
}
