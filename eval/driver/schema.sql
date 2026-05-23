-- mpfr-ts — state.db schema (v1)
--
-- This file is the single source of truth for the eval state DB layout.
-- It is applied once when the DB is created (see CLAUDE.md §"Build & test"):
--
--     sqlite3 eval/state.db < eval/driver/schema.sql
--
-- It is idempotent: every CREATE uses IF NOT EXISTS, and the seed INSERT
-- uses INSERT OR IGNORE. Re-running against an existing DB is a no-op.
--
-- Schema versioning: the `meta` table stamps `schema_version`. Future
-- migrations must bump this AND ship as a separate migration script
-- (do not silently mutate this file). See Rule 9 (state.db is the only
-- persistent tracker; conversation context is not).

-- ---------------------------------------------------------------------------
-- Connection-level pragmas
-- ---------------------------------------------------------------------------
--
-- foreign_keys MUST be enabled per connection — SQLite defaults to OFF for
-- backwards compatibility. Every tool that opens this DB (sqlite3 shell,
-- Python's sqlite3, Bun's bun:sqlite, etc.) must issue this PRAGMA itself;
-- setting it here only affects the schema-apply session. Documented here
-- so future maintainers know not to remove it from the runtime code paths.
PRAGMA foreign_keys = ON;

-- journal_mode = WAL is a PERSISTENT property of the DB file (unlike
-- foreign_keys), so setting it once during creation is sufficient. WAL
-- gives us concurrent readers (e.g. dashboard.py querying while ralph.py
-- writes a new run). Trade-off: produces sidecar files `state.db-wal` and
-- `state.db-shm` next to the DB; these are ephemeral and must be
-- gitignored (the canonical state.db file is what gets committed).
PRAGMA journal_mode = WAL;

-- ---------------------------------------------------------------------------
-- functions: one row per MPFR function we plan to port
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS functions (
  -- Stable identifier; matches the C name (e.g. 'mpn_add_n', 'mpfr_add').
  -- The TS port may rename this in the public API (e.g. mpfr_set_d → setD)
  -- but the C name is the join key everywhere.
  name TEXT PRIMARY KEY,

  -- Coarse classification used by ralph.py to pick prompt templates and by
  -- the dashboard to bucket progress. CHECK guards against typo-classes
  -- (e.g. 'arithmatic') silently flowing through the loop.
  class TEXT NOT NULL CHECK (class IN (
    'arithmetic', 'conversion', 'transcendental', 'misc', 'substrate'
  )),

  -- TS signature copy-pasted from src/core.ts conventions. Stored as TEXT
  -- so the prompt template can splice it verbatim without parsing.
  signature TEXT NOT NULL,

  -- JSON array of fn names this function depends on (callees in the C
  -- call graph). Format: '["mpn_add_n","mpn_cmp"]'. Empty leaf functions
  -- store '[]'. Queried via json_each (see CLAUDE.md §"Common queries").
  deps TEXT NOT NULL,

  -- Lifecycle state. CHECK enumerates the only legal transitions; agents
  -- writing 'inflight' (no underscore) or 'PARKED' (uppercase) fail loudly.
  status TEXT NOT NULL CHECK (status IN (
    'pending', 'in_flight', 'done', 'slow', 'parked', 'blocked'
  )),

  -- FK to the best (highest composite) run for this function. Intentionally
  -- NULLABLE with NO FOREIGN KEY constraint: a chicken-and-egg cycle exists
  -- with runs.fn_name → functions.name. SQLite supports DEFERRABLE INITIALLY
  -- DEFERRED, but that requires `PRAGMA defer_foreign_keys = ON` per
  -- transaction, and any tool that forgets it gets cryptic FK errors. The
  -- inverse direction (runs.fn_name → functions.name) is the one we actually
  -- need enforced; this column is set by application code after a run lands,
  -- and a stale best_run_id only ever points to a soft-deleted run (which
  -- ON DELETE CASCADE on cases/runs makes self-healing). Pragmatic choice
  -- over the "correct" deferred-FK choice because the latter is a footgun.
  best_run_id TEXT,

  -- Composite correctness of the best run. NULL until first run lands.
  -- Range 0.0..1.0 — anything outside this is a grader bug.
  best_correctness REAL CHECK (
    best_correctness IS NULL OR (best_correctness BETWEEN 0.0 AND 1.0)
  ),

  -- Perf grade of the best run. NULL until first run. Range 0.0..1.0 by
  -- convention (1.0 == matches C reference timing), though in theory a
  -- port can exceed 1.0 if it beats the C reference. We keep the upper
  -- bound at 1.0 on the FUNCTIONS aggregate (clamped by the writer) but
  -- allow > 1.0 on the per-run column below.
  best_perf_grade REAL CHECK (
    best_perf_grade IS NULL OR (best_perf_grade BETWEEN 0.0 AND 1.0)
  ),

  -- Number of port attempts; CHECK catches negative writes from broken
  -- counter math (e.g. an "increment" that subtracts).
  attempts INTEGER NOT NULL DEFAULT 0 CHECK (attempts >= 0),

  -- 0 = still on sonnet L3; 1 = escalated to opus L3 at least once.
  -- Strict 0/1 boolean (SQLite has no native bool); CHECK rejects '2' or 'true'.
  escalated INTEGER NOT NULL DEFAULT 0 CHECK (escalated IN (0, 1)),

  -- Sort key for the ralph loop's "next function" picker. Lower = picked
  -- first. Populated from the C call graph topo sort (eval/driver/callgraph.py).
  topo_rank INTEGER NOT NULL
);

-- ---------------------------------------------------------------------------
-- runs: one row per (function, grader invocation)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS runs (
  -- Opaque ID assigned by the runner (e.g. 'r2026-05-23T14:02:11Z-abc123').
  -- Used as the FK target from cases and as best_run_id in functions.
  run_id TEXT PRIMARY KEY,

  -- FK to the function this run graded. ON DELETE CASCADE keeps the DB
  -- consistent if a function row is deleted (rare; mostly during pilot
  -- iteration). NOT NULL: an orphan run makes no sense.
  fn_name TEXT NOT NULL REFERENCES functions(name) ON DELETE CASCADE,

  -- Model + effort tier (e.g. 'sonnet', 'L3'). String columns rather than
  -- enums because the model lineup will change faster than the schema.
  model TEXT NOT NULL,
  effort TEXT NOT NULL,

  -- RNG seed for the fuzz cases — reproducibility.
  seed INTEGER NOT NULL,

  -- Wall-clock timestamps, unix seconds with subsecond precision.
  started_at REAL NOT NULL,
  ended_at REAL NOT NULL CHECK (ended_at >= started_at),

  -- Composite correctness in [0, 1]. Definitionally cannot exceed 1.0.
  composite_correctness REAL NOT NULL CHECK (
    composite_correctness BETWEEN 0.0 AND 1.0
  ),

  -- Perf grade. Lower bound 0; NO upper bound because a port that beats
  -- the C reference timing legitimately scores > 1.0. (Unlikely for naive
  -- TS-vs-C, but possible on heavy-allocator paths where BigInt outperforms
  -- mpn malloc churn.)
  perf_grade REAL NOT NULL CHECK (perf_grade >= 0.0),

  -- Per-bucket case counts. CHECKs enforce: non-negative, and the
  -- individual buckets cannot exceed the total. (n_pass + n_throw +
  -- n_timegate + n_infloop need not sum to n_cases — a case can be
  -- "passed AND fast" or "failed AND fast" — but each bucket alone is
  -- bounded by n_cases.)
  n_cases INTEGER NOT NULL CHECK (n_cases >= 0),
  n_pass INTEGER NOT NULL CHECK (n_pass >= 0 AND n_pass <= n_cases),
  n_throw INTEGER NOT NULL CHECK (n_throw >= 0 AND n_throw <= n_cases),
  n_timegate INTEGER NOT NULL CHECK (n_timegate >= 0 AND n_timegate <= n_cases),
  n_infloop INTEGER NOT NULL CHECK (n_infloop >= 0 AND n_infloop <= n_cases),

  -- Freeform diagnostic string — first error message surfaced by the
  -- grader (truncated by the writer if very long). NULL if all cases
  -- passed. Used by the "parked: what blocked us?" dashboard query.
  first_error TEXT,

  -- Filesystem paths to the raw agent output, the extracted port file,
  -- and the grade.json. Stored as TEXT (relative or absolute, writer's
  -- choice). NULLable: not every run produces all three (a schema-violation
  -- rejection short-circuits before a grade.json exists).
  raw_path TEXT,
  port_path TEXT,
  grade_path TEXT,

  -- Estimated USD cost of this run; ±2× heuristic per CLAUDE.md.
  -- Relative ordering between runs is sound, absolute dollars are not.
  usd_est REAL NOT NULL CHECK (usd_est >= 0.0)
);

-- ---------------------------------------------------------------------------
-- cases: per-test-case detail for a run
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS cases (
  -- Composite PK with run_id; ON DELETE CASCADE so deleting a run
  -- (e.g. discarding a corrupted grader output) drops its cases atomically.
  run_id TEXT NOT NULL REFERENCES runs(run_id) ON DELETE CASCADE,

  -- 0-based index within the golden file. NOT NULL.
  case_idx INTEGER NOT NULL CHECK (case_idx >= 0),

  -- Tag class — happy|edge|adversarial|fuzz|mined. Free-form TEXT (not
  -- enumerated via CHECK) because new tag classes may appear during
  -- Production (e.g. 'integration'); enforcing here would require schema
  -- bumps for every addition.
  tag TEXT NOT NULL,

  -- 0/1 booleans for the three outcome bits. A case can be (passed=1,
  -- threw=0, timed_out=0); (passed=0, threw=1, timed_out=0);
  -- (passed=0, threw=0, timed_out=1); or (passed=0, threw=0, timed_out=0)
  -- meaning "ran to completion but produced the wrong answer". The
  -- combination (passed=1 AND timed_out=1) is nonsense — the CHECK guards it.
  passed INTEGER NOT NULL CHECK (passed IN (0, 1)),
  threw INTEGER NOT NULL CHECK (threw IN (0, 1)),
  timed_out INTEGER NOT NULL CHECK (timed_out IN (0, 1)),

  -- Actual ms vs the worker's per-case budget (Rule 4: 50/200/1000 ms
  -- by class tier). Both non-negative; ms_actual may exceed ms_budget
  -- when timed_out=1 (the worker.terminate() takes a moment to settle).
  ms_actual REAL NOT NULL CHECK (ms_actual >= 0.0),
  ms_budget REAL NOT NULL CHECK (ms_budget > 0.0),

  PRIMARY KEY (run_id, case_idx),

  -- A case can be (passed=1, threw=0, timed_out=0); (passed=0, threw=1,
  -- timed_out=0); (passed=0, threw=0, timed_out=1); or (passed=0, threw=0,
  -- timed_out=0) meaning "ran to completion but produced the wrong answer".
  -- The combination (passed=1 AND timed_out=1) or (passed=1 AND threw=1) is
  -- nonsense — this table-level CHECK guards it.
  CHECK (NOT (passed = 1 AND (threw = 1 OR timed_out = 1)))
);

-- ---------------------------------------------------------------------------
-- meta: schema version + future key/value config
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS meta (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
);

-- INSERT OR IGNORE so re-running this script doesn't bump or duplicate.
-- Migrations bump this value as their first DML.
INSERT OR IGNORE INTO meta (key, value) VALUES ('schema_version', '1');

-- ---------------------------------------------------------------------------
-- Indexes — backing the queries in CLAUDE.md §"Common queries"
-- ---------------------------------------------------------------------------
-- "ralph loop's next pick": filters by status, orders by topo_rank.
CREATE INDEX IF NOT EXISTS idx_functions_status ON functions(status);
CREATE INDEX IF NOT EXISTS idx_functions_topo   ON functions(topo_rank);

-- "runs for a function": foreign-key column needs an index; SQLite does
-- not auto-create one for FK columns the way some other engines do.
CREATE INDEX IF NOT EXISTS idx_runs_fn ON runs(fn_name);
