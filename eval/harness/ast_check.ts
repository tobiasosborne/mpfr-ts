/**
 * ast_check.ts — schema-violation gate for ported TS files.
 *
 * Pure function, no I/O. Inspects a port's source text and rejects the
 * port outright (composite=0) if it would violate Law 4 (Library Coherence)
 * or shows a quality-regression signal. The checks are deliberately
 * regex-based: a real TS parser is overkill for the gate, and we want
 * cheap-to-evaluate, cheap-to-tune patterns we can ratchet up over time.
 *
 * Checks enforced:
 *
 *   1. (opt-in) The port imports at least one symbol from `core.ts`.
 *      Substrate ports under `src/internal/mpn/` are exempt because they
 *      deal in bigint arrays, not MPFR values — pass
 *      `requireCoreImport: false` for those.
 *
 *   2. The port MUST NOT redeclare any of the locked schema types:
 *      `MPFR`, `RoundingMode`, `Result`, `Ternary`, `MPFRError`. Word
 *      boundaries are required so `MPFRError` doesn't trigger the `MPFR`
 *      pattern.
 *
 *   3. The port MUST NOT use `any` (`: any`, `as any`). The auto-port-eval
 *      predecessor showed that even "cheap" `any` annotations propagate
 *      grader-invisible type errors.
 *
 *   4. The port MUST NOT contain Cyrillic or Greek confusable characters
 *      (U+0370–U+04FF). Auto-port-eval lost a grade to a Cyrillic 'а' in
 *      a hex literal (`0xaaaa...а`); the file looked fine in the
 *      terminal, imported with SyntaxError. (CLAUDE.md Rule 13.)
 *
 * Comments cite line numbers where useful. Errors are returned, never
 * thrown — the grader records them into grade.json.
 *
 * Ref: CLAUDE.md §"Library coherence" — the enforcement contract.
 * Ref: CLAUDE.md Rule 13 — homoglyph check rationale.
 */

export interface AstCheckResult {
  readonly ok: boolean;
  readonly errors: readonly string[];
  /** Symbols (or `*` for wildcard re-exports) the port imports from core.ts. */
  readonly coreImports: readonly string[];
}

export interface AstCheckOptions {
  /**
   * When true, the port must import at least one symbol from a path ending
   * in `core.ts`. Set false for substrate ports under `src/internal/mpn/`
   * which legitimately speak in raw bigint arrays.
   */
  readonly requireCoreImport: boolean;
}

// ---------------------------------------------------------------------------
// Patterns
// ---------------------------------------------------------------------------

/**
 * Matches an import statement with a from-clause whose path ends in
 * `core.ts`. Captures the brace contents in group 1 (for symbol extraction).
 *
 * Permissive on whitespace and on the optional `type` keyword
 * (`import type {...}`), to accommodate both `import {X} from "../core.ts"`
 * and `import { X, Y, } from "../../core.ts"`. Anchored at the `from` to
 * avoid matching unrelated `{...}` blocks elsewhere.
 *
 * Does NOT match wildcard imports (`import * as core from "core.ts"`); we
 * scan those separately so we can record the wildcard explicitly.
 */
const NAMED_CORE_IMPORT_RE =
  /import\s+(?:type\s+)?\{([^}]+)\}\s+from\s+['"]((?:[^'"]*\/)?core\.ts)['"]/g;

/**
 * Matches a wildcard import from a core.ts path. Records as `*`.
 */
const WILDCARD_CORE_IMPORT_RE =
  /import\s+\*\s+as\s+\w+\s+from\s+['"](?:[^'"]*\/)?core\.ts['"]/g;

/**
 * Matches a default-or-namespace import that goes through core.ts but
 * doesn't actually destructure anything. Reported as a warning-style symbol
 * `default` (defensive — core.ts doesn't have a default export so this is
 * always a bug, but we surface it as an import rather than a violation).
 */
const DEFAULT_CORE_IMPORT_RE =
  /import\s+(\w+)\s+from\s+['"](?:[^'"]*\/)?core\.ts['"]/g;

/**
 * Locked-schema redeclaration patterns. Word-boundary on the type name so:
 *
 *   - `interface MPFR { ... }`   matches   `MPFR`  but not `MPFRError`
 *     (boundary after R blocks the longer name).
 *   - `class MPFRError`          matches   `MPFRError` only.
 *
 * The `\\b` after the name is required; without it `MPFR` matches inside
 * `MPFRError` and produces a false positive for every well-behaved file.
 */
const REDECL_PATTERNS: ReadonlyArray<{ readonly name: string; readonly re: RegExp }> = [
  { name: 'MPFR', re: /(?:^|[^.\w])(?:interface|type)\s+MPFR\b/m },
  { name: 'RoundingMode', re: /(?:^|[^.\w])(?:type|enum)\s+RoundingMode\b/m },
  { name: 'Result', re: /(?:^|[^.\w])(?:interface|type)\s+Result\b/m },
  { name: 'Ternary', re: /(?:^|[^.\w])type\s+Ternary\b/m },
  { name: 'MPFRError', re: /(?:^|[^.\w])class\s+MPFRError\b/m },
  { name: 'MPFRKind', re: /(?:^|[^.\w])type\s+MPFRKind\b/m },
];

/**
 * `any` usage patterns. Catches the two most common forms; we don't try to
 * catch every conceivable `any` (e.g. in a type alias) because the strict
 * tsconfig will reject those at type-check time. This gate is for the
 * subset that slips past `verbatimModuleSyntax` strictness.
 */
const ANY_PATTERNS: ReadonlyArray<{ readonly label: string; readonly re: RegExp }> = [
  { label: ': any annotation', re: /:\s*any\b/ },
  { label: 'as any cast', re: /\bas\s+any\b/ },
  { label: '<any> generic', re: /<\s*any\s*[,>]/ },
  { label: 'Array<any>', re: /Array\s*<\s*any\s*>/ },
];

/**
 * Confusable-character codepoint ranges. Both Greek (U+0370–U+03FF) and
 * Cyrillic (U+0400–U+04FF) contain visual look-alikes for ASCII letters
 * the agent might paste through a multilingual transformer. We do NOT
 * reject characters outside these ranges (emoji, CJK, etc.) since they're
 * obvious in code review; the failure mode we're guarding against is
 * specifically "indistinguishable from ASCII".
 *
 * Ref: ../auto-port-eval/HANDOFF.md §"Things I learned the hard way" #1.
 */
function isConfusableChar(codepoint: number): boolean {
  // Greek and Coptic + Cyrillic + Cyrillic Supplement.
  return (
    (codepoint >= 0x0370 && codepoint <= 0x03ff) ||
    (codepoint >= 0x0400 && codepoint <= 0x04ff)
  );
}

/**
 * Strip `import … from "…";` statements from the source so the
 * REDECL_PATTERNS don't false-flag the `type` keyword inside an import
 * specifier list (`import { type MPFR, … }`). Handles single-line and
 * multi-line forms. We replace the matched span with newlines so line
 * numbers reported elsewhere stay stable.
 *
 * The pattern is intentionally narrow: it matches only `import` followed
 * by anything up to and including the closing quote of the source
 * specifier (`from "…"` or `from '…'`) plus an optional trailing
 * semicolon. Side-effect imports (`import "polyfill";`) and bare
 * dynamic-import calls (`import("./x.ts")`) don't contain `type` keywords
 * before any of the locked names, but we still match them to be safe.
 *
 * Note: this is a regex strip, not an AST walk. The contract we rely on
 * is "an `import` token at a statement boundary begins an import
 * declaration until the next source-specifier-string". A pathological
 * file with a string literal containing the substring `import { type
 * MPFR } from "…"` would slip past, but ports don't generate such
 * strings, and the alternative — full TS-parsing — is overkill.
 *
 * Ref: mpfr-ts-wli (the false-positive), worklog 005 §3 (the fix
 *   choice).
 */
const IMPORT_STATEMENT_RE =
  /\bimport\b(?:[^'"\n;]|\n)*?\bfrom\s*['"][^'"]*['"]\s*;?|\bimport\s*['"][^'"]*['"]\s*;?/g;

function stripImportStatements(source: string): string {
  return source.replace(IMPORT_STATEMENT_RE, (match) => {
    // Preserve newlines so line-number-sensitive code downstream stays
    // aligned. Replace every non-newline char with a space.
    return match.replace(/[^\n]/g, ' ');
  });
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

/**
 * Run all enabled checks against the port source. Returns an
 * {@link AstCheckResult}; never throws. Multiple violations accumulate;
 * we surface them all so the user sees the full picture rather than one
 * fix-rerun-repeat cycle per defect.
 */
export function astCheck(
  source: string,
  opts: AstCheckOptions,
): AstCheckResult {
  const errors: string[] = [];
  const coreImports: string[] = [];

  // --- (1) Core import requirement -----------------------------------------
  let m: RegExpExecArray | null;
  NAMED_CORE_IMPORT_RE.lastIndex = 0;
  while ((m = NAMED_CORE_IMPORT_RE.exec(source)) !== null) {
    const braces = m[1] ?? '';
    // Split on comma, strip `type ` prefix and aliases (`X as Y`), trim ws.
    const symbols = braces
      .split(',')
      .map((s) => s.trim())
      .filter((s) => s.length > 0)
      .map((s) => s.replace(/^type\s+/, ''))
      .map((s) => s.split(/\s+as\s+/)[0] ?? s)
      .map((s) => s.trim());
    coreImports.push(...symbols);
  }
  WILDCARD_CORE_IMPORT_RE.lastIndex = 0;
  while ((m = WILDCARD_CORE_IMPORT_RE.exec(source)) !== null) {
    coreImports.push('*');
  }
  DEFAULT_CORE_IMPORT_RE.lastIndex = 0;
  while ((m = DEFAULT_CORE_IMPORT_RE.exec(source)) !== null) {
    const name = m[1] ?? 'default';
    // Record for diagnostic visibility, but flag as an explicit error:
    // core.ts has no default export, so any default-shaped import from it
    // is necessarily a bug. We do NOT let this satisfy requireCoreImport.
    coreImports.push(`default(${name})`);
    errors.push(
      `default import from core.ts ('${name}'): core.ts has no default ` +
        `export — import named symbols instead (e.g. \`import { MPFR } from "../core.ts"\`)`,
    );
  }

  // `default(...)` entries are recorded for diagnostics but must not
  // satisfy the core-import requirement: a port whose only "core import"
  // is a bogus default import has not actually imported the schema.
  const namedCoreImports = coreImports.filter((s) => !s.startsWith('default('));
  if (opts.requireCoreImport && namedCoreImports.length === 0) {
    errors.push(
      'missing required import from src/core.ts (Law 4: every public port ' +
        'must import MPFR / RoundingMode / Result from the locked schema)',
    );
  }

  // --- (2) Redeclaration of locked schema types ----------------------------
  // The redeclaration patterns are deliberately fuzzy ("(?:^|[^.\w])type\s+
  // MPFR\b") so they catch top-level `type MPFR = …` as well as indented
  // ones. The same fuzziness false-flags the mixed type-import syntax —
  // `import { type MPFR, … } from "../core.ts"` — because `{` is a
  // non-word character before `type`, satisfying the `[^.\w]` anchor.
  // Stripping import statements (single- and multi-line) before the scan
  // is safe: imports cannot contain a real type/interface/class/enum
  // declaration, only a re-export of names. We keep the original source
  // for downstream checks (`any`, Cyrillic) so import-path text still
  // gets the homoglyph scan. Ref: mpfr-ts-wli / worklog 005.
  const importStripped = stripImportStatements(source);
  for (const pat of REDECL_PATTERNS) {
    if (pat.re.test(importStripped)) {
      errors.push(
        `redeclares locked schema type '${pat.name}' (Law 4: import from ` +
          `src/core.ts; do not redeclare)`,
      );
    }
  }

  // --- (3) `any` usage -----------------------------------------------------
  // Apply the `any` patterns to a copy of the source with comments and
  // string/template literals erased. Without this step the gate falsely
  // flags `// no `any` here` in a JSDoc, or the literal string `": any"`
  // in an error message. We deliberately do NOT use this stripped form
  // for the Cyrillic scan below — Rule 13 wants to catch homoglyphs in
  // string literals too (the `0xaaaaaaaaа` failure mode was literally a
  // hex literal, which is not a string, but a Cyrillic char inside a
  // user-facing error message would still ship and confuse downstream
  // users). Issue: mpfr-ts-4hp.
  const stripped = source
    // Block comments first (non-greedy, multi-line). Done before line
    // comments so `/* // */` isn't mis-split.
    .replace(/\/\*[\s\S]*?\*\//g, '')
    // Line comments to end-of-line.
    .replace(/\/\/[^\n]*/g, '')
    // Template literals — replace contents with empty backticks. Does NOT
    // recurse into `${...}` expressions; an `: any` inside a template
    // interpolation would slip through, but that's vanishingly unlikely
    // in port code and false-negative-prone is acceptable here.
    .replace(/`[\s\S]*?`/g, '``')
    // Double-quoted strings, with escape-aware matching.
    .replace(/"(?:\\.|[^"\\])*"/g, '""')
    // Single-quoted strings, with escape-aware matching.
    .replace(/'(?:\\.|[^'\\])*'/g, "''");

  for (const pat of ANY_PATTERNS) {
    if (pat.re.test(stripped)) {
      errors.push(
        `uses '${pat.label}' — ports must use 'unknown' + narrowing instead`,
      );
    }
  }

  // --- (4) Cyrillic / Greek homoglyph scan ---------------------------------
  // Single-pass: track line number explicitly. Cheap enough that we don't
  // bother with .indexOf seeking. Reports the first occurrence per line to
  // avoid spamming the errors list if the same char appears repeatedly.
  let line = 1;
  let reportedOnLine = -1;
  for (let i = 0; i < source.length; i++) {
    const ch = source.charCodeAt(i);
    if (ch === 0x0a /* \n */) {
      line++;
      continue;
    }
    if (isConfusableChar(ch) && reportedOnLine !== line) {
      const hex = ch.toString(16).toUpperCase().padStart(4, '0');
      const char = source.charAt(i);
      errors.push(
        `non-ASCII confusable character U+${hex} ('${char}') on line ${line} ` +
          `(Cyrillic/Greek look-alike — CLAUDE.md Rule 13)`,
      );
      reportedOnLine = line;
    }
  }

  return {
    ok: errors.length === 0,
    errors,
    coreImports,
  };
}
