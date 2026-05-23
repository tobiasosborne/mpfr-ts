/**
 * worker.ts — per-case isolation worker for the grader.
 *
 * Bun spawns one of these per pool slot via `new Worker(new URL(...))` from
 * runner.ts. The protocol is:
 *
 *   1. Parent sends one `init` message with `{ portUrl, functionName }`.
 *      The worker dynamically imports the port and resolves the exported
 *      function once, then replies `{ type: 'ready' }` (or `{ type: 'init_error' }`
 *      if the import or export lookup failed).
 *
 *   2. Parent sends `{ type: 'case', caseIdx, inputArgs }` messages. The
 *      worker invokes the function synchronously (we don't await; the port
 *      contract is sync), times the call with `performance.now()` bracketing
 *      ONLY the call (not the import), and replies:
 *
 *        { type: 'result', caseIdx, ms, ok: true,  value }
 *        { type: 'result', caseIdx, ms, ok: false, error }
 *
 * The worker enforces NO timeouts of its own — the parent races each
 * message against a setTimeout and calls `worker.terminate()` on expiry.
 * This is the only design that survives synchronous infinite loops in the
 * ported code; a worker-internal AbortController cannot interrupt a
 * synchronous busy-loop because the event loop never yields.
 *
 * The parent must also time-bound the init -> ready round-trip; a port
 * with top-level await or top-level infinite work blocks `await import`
 * and will never reply. Runner.ts owns this timeout.
 *
 * The worker does NO value comparison — that's the runner's job; the
 * worker just returns whatever the port returned (BigInts and all, since
 * Bun's `postMessage` uses structured clone which handles BigInt natively).
 *
 * Ref: CLAUDE.md Rule 4 — per-test worker isolation, parent-side terminate
 *   on timeout.
 * Ref: ../auto-port-eval/HANDOFF.md §2 — "Infinite loops in ported code"
 *   was the predecessor's single largest known failure mode; this worker
 *   is the fork that addresses it.
 */

/// <reference lib="webworker" />

// The `Worker` global type from webworker has a `self` typed as
// `WorkerGlobalScope & typeof globalThis`, which covers what we need. We
// import nothing from the project; this file is loaded as a Worker module
// by Bun and only the protocol with the parent matters.

// ---------------------------------------------------------------------------
// Message types — duplicated from runner.ts deliberately. The runner is the
// only consumer; we could share a types file, but having the protocol
// documented in two places means a runner-side change shows up as a worker-
// side compile error in the next CI tick. (We don't have CI today, but the
// principle stands and the duplication is ~10 lines.)
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

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

// Resolved once on init; reused for every subsequent case. Stored on the
// closure rather than via `globalThis` so we don't need a cast.
let portFunction: ((...args: readonly unknown[]) => unknown) | null = null;

/**
 * Type-narrow that the global `postMessage` exists. Webworker types provide
 * this but verbatimModuleSyntax + `types: []` in tsconfig means we have to
 * be defensive.
 */
declare function postMessage(message: ChildMessage): void;

// ---------------------------------------------------------------------------
// Message handler
// ---------------------------------------------------------------------------

self.onmessage = async (evt: MessageEvent<ParentMessage>) => {
  const msg = evt.data;
  if (msg.type === 'init') {
    try {
      const mod: Readonly<Record<string, unknown>> = await import(msg.portUrl);
      const candidate = mod[msg.functionName] ?? mod['default'];
      if (typeof candidate !== 'function') {
        const exportNames = Object.keys(mod).join(',');
        postMessage({
          type: 'init_error',
          error: `no export named '${msg.functionName}' (found: ${exportNames})`,
        });
        return;
      }
      portFunction = candidate as (...args: readonly unknown[]) => unknown;
      postMessage({ type: 'ready' });
    } catch (e) {
      postMessage({
        type: 'init_error',
        error: `import failed: ${e instanceof Error ? e.message : String(e)}`,
      });
    }
    return;
  } else if (msg.type === 'case') {
    if (portFunction === null) {
      // Defensive: should not happen given the parent waits for 'ready'
      // before sending cases, but a malformed parent or a race during
      // worker respawn could trigger this. Reply with an error rather
      // than silently dropping the case.
      postMessage({
        type: 'result',
        caseIdx: msg.caseIdx,
        ms: 0,
        ok: false,
        error: 'worker received case before init',
      });
      return;
    }
    const fn = portFunction;
    const t0 = performance.now();
    try {
      const value = fn(...msg.inputArgs);
      // Reject async/thenable returns: the port contract is strictly
      // synchronous, and a Promise crossing structured-clone surfaces as a
      // DataCloneError that masks the real contract violation. Throwing
      // here lets the existing catch reply with a precise diagnostic.
      // Issue: mpfr-ts-eep.
      if (
        value !== null &&
        typeof value === 'object' &&
        typeof (value as { then?: unknown }).then === 'function'
      ) {
        throw new Error(
          'port returned a Promise; sync contract violated ' +
            '(await is not supported in port functions)',
        );
      }
      const ms = performance.now() - t0;
      // Note: structured clone serialises BigInt, plain objects, arrays of
      // BigInt, etc. — no special handling needed on either side.
      postMessage({ type: 'result', caseIdx: msg.caseIdx, ms, ok: true, value });
    } catch (e) {
      const ms = performance.now() - t0;
      postMessage({
        type: 'result',
        caseIdx: msg.caseIdx,
        ms,
        ok: false,
        error: e instanceof Error ? e.message : String(e),
      });
    }
    return;
  } else {
    // Exhaustiveness: ParentMessage is InitMessage | CaseMessage. If a new
    // variant is added without a handler, the `never` assignment fails at
    // compile time. The runtime postMessage covers a misbehaving parent
    // that posts an unknown shape past the type system. Issue: mpfr-ts-5gz.
    const _exhaustive: never = msg;
    postMessage({
      type: 'init_error',
      error: `worker received unknown message type: ${JSON.stringify(_exhaustive)}`,
    });
    return;
  }
};
