/*
 * mpfr-ts golden-master shared utilities.
 *
 * One C header per the eval/golden_master/ contract: every per-function
 * golden driver under eval/functions/<fn>/golden_driver.c includes this
 * file and uses its JSONL emit helpers. The wire format produced here is
 * what the TS harness deserialises in eval/harness/value_codec.ts; the
 * MPFR value shape must round-trip through src/core.ts's validate().
 *
 * Build with -lmpfr -lgmp -lm. See build.sh.
 *
 * Design notes
 * ------------
 *
 *   * Wire format is JSONL (one JSON record per line) of the form
 *
 *         {"tag":"<class>","inputs":{...},"output":<value>,"time_ns":<n>}
 *
 *     where <class> is one of happy/edge/adversarial/fuzz/mined (per
 *     CLAUDE.md Rule 7) and inputs is a *named* object (not the array
 *     used by the FLINT predecessor in auto-port-eval), because MPFR
 *     inputs are heterogeneous structs whose ordering matters and which
 *     deserve named keys.
 *
 *   * Integer-valued fields (limbs, mantissas, precisions, exponents)
 *     are emitted as decimal strings so the TS side can pass them to
 *     BigInt() without parseInt overflow worries.
 *
 *   * Rounding modes are emitted as the literal strings the TS side
 *     uses ("RNDN" etc.) — never as the underlying MPFR int enum, so
 *     no enum-drift between sides.
 *
 *   * All helpers are `static inline` so users get zero link-time cost
 *     from #include alone. No global or file-static state anywhere;
 *     every helper takes its FILE* and a `first` flag.
 *
 * Ref: src/core.ts — locked MPFR/RoundingMode/Result types.
 * Ref: CLAUDE.md §"Library coherence" — wire format expectations.
 * Ref: ../auto-port-eval/eval/golden_master/common.h — the FLINT
 *   predecessor this is adapted from.
 */
#ifndef MPFR_TS_GOLDEN_MASTER_COMMON_H
#define MPFR_TS_GOLDEN_MASTER_COMMON_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L  /* clock_gettime, CLOCK_MONOTONIC */
#endif

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <gmp.h>
#include <mpfr.h>

/* ------------------------------------------------------------------ */
/* B. PRNG — xorshift64                                               */
/* ------------------------------------------------------------------ */

/* xorshift64 state. Identical algorithm to auto-port-eval so seed N
 * here produces the same stream as seed N there: reproducible goldens
 * forever. */
typedef struct { uint64_t s; } xs64_t;

/* Seed the PRNG. Zero is replaced by Knuth's multiplicative constant
 * because xorshift on an all-zero state is a fixed point. */
static inline void xs64_seed(xs64_t *r, uint64_t seed) {
    r->s = seed ? seed : 0x9E3779B97F4A7C15ULL;
}

/* Advance and return the next 64-bit pseudo-random value. */
static inline uint64_t xs64_next(xs64_t *r) {
    uint64_t x = r->s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    r->s = x;
    return x;
}

/* Return a uniform-ish value in [0, bound). Biased for non-power-of-2
 * bounds, but goldens don't care about the last bit of fairness. */
static inline uint64_t xs64_below(xs64_t *r, uint64_t bound) {
    if (bound == 0) return 0;
    return xs64_next(r) % bound;
}

/* ------------------------------------------------------------------ */
/* C. Timing                                                          */
/* ------------------------------------------------------------------ */

/* Monotonic nanoseconds. Used to populate the per-record "time_ns"
 * field, which the runner uses to set per-test budgets. */
static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* ------------------------------------------------------------------ */
/* D. JSONL emit — record framing                                     */
/* ------------------------------------------------------------------ */

/* Begin a record: writes `{"tag":"<tag>","inputs":{`. After this the
 * caller emits each input with jl_kv_<type>, threading a `first` flag
 * (1 on the first call, 0 afterwards) to manage comma placement. */
static inline void jl_begin(FILE *f, const char *tag) {
    fprintf(f, "{\"tag\":\"%s\",\"inputs\":{", tag);
}

/* Close the inputs object. Must be called once between the last
 * jl_kv_* and the first jl_output_*. */
static inline void jl_end_inputs(FILE *f) { fputs("}", f); }

/* Close the record with the timing field. The leading comma is
 * unconditional because output is always present. */
static inline void jl_finish(FILE *f, uint64_t time_ns) {
    fprintf(f, ",\"time_ns\":%" PRIu64 "}\n", time_ns);
}

/* ------------------------------------------------------------------ */
/* Internal: comma + key prefix                                       */
/* ------------------------------------------------------------------ */

/* Emit `"key":` with a leading comma iff !first. Centralising this
 * keeps every jl_kv_* helper consistent and one-liner. */
static inline void jl__key(FILE *f, int first, const char *key) {
    fprintf(f, "%s\"%s\":", first ? "" : ",", key);
}

/* ------------------------------------------------------------------ */
/* E. Scalar JSON helpers                                             */
/* ------------------------------------------------------------------ */

/* Emit ,"key":"<decimal>" — uint64 as a decimal string for BigInt
 * round-trip on the TS side. */
static inline void jl_kv_u64(FILE *f, int first, const char *key, uint64_t v) {
    jl__key(f, first, key);
    fprintf(f, "\"%" PRIu64 "\"", v);
}

/* Emit ,"key":"<decimal>" — int64 as a decimal string. */
static inline void jl_kv_i64(FILE *f, int first, const char *key, int64_t v) {
    jl__key(f, first, key);
    fprintf(f, "\"%" PRId64 "\"", v);
}

/* Emit ,"key":N — raw int, NOT stringified. Use for small integer
 * results that fit comfortably in a JS Number (ternary flag, array
 * length, etc.). Prefer named string forms for enums (see
 * jl_kv_rnd). */
static inline void jl_kv_int(FILE *f, int first, const char *key, int v) {
    jl__key(f, first, key);
    fprintf(f, "%d", v);
}

/* Emit ,"key":"<token>" where <token> is one of "NaN" / "+Infinity" /
 * "-Infinity" / a "%.17g" lossless decimal — for the IEEE 754 double
 * input type used by ports like mpfr_set_d.
 *
 * Why a quoted string and not a bare JSON number? Because JSON cannot
 * encode NaN or ±Infinity as bare numbers (the grammar in RFC 8259
 * forbids it), and emitting `NaN` unquoted produces a parse error in
 * standards-compliant decoders — including JSON.parse in V8/Bun.
 * Quoting every double — finite or not — keeps the wire format uniform
 * so the TS-side decoder needs a single rule.
 *
 * Round-trip contract: "%.17g" is the IEEE 754 round-trip-safe
 * specifier for `double` — every finite normal/subnormal double `d`
 * satisfies `Number(string(d)) === d` after this format, by IEEE 754
 * §5.12.2 (17 significant decimal digits suffice). The TS-side decoder
 * in value_codec.ts uses `Number(s)` (NOT parseFloat — parseFloat
 * tolerates trailing junk; Number is strict).
 *
 * Ref: src/core.ts L19–L63 — value model; the double extraction in
 *   the TS port reads sign/exp/mant directly from a DataView.
 * Ref: CLAUDE.md "Hallucination-risk callouts" — NaN ≠ NaN; signed
 *   zero observable. We emit +0 and -0 as "0" and "-0" via "%.17g".
 *   The TS-side `Number("-0") === -0` and `Object.is(Number("-0"), -0)`
 *   is true under V8/Bun, so the signed-zero distinction survives.
 */
static inline void jl_kv_double(FILE *f, int first, const char *key, double v) {
    jl__key(f, first, key);
    /* NaN check first — DOUBLE_ISNAN is a libmpfr macro but we can't
     * count on it here (common.h is consumed by drivers that may not
     * have included <math.h>). Use the IEEE-754 self-inequality trick:
     * NaN is the only value that compares unequal to itself. */
    if (v != v) {
        fputs("\"NaN\"", f);
        return;
    }
    if (v > 0 && v * 0.5 == v) {
        /* +inf: doubling (halving) leaves it unchanged. Faster than
         * including <math.h> for isinf, and works on every IEEE-754
         * implementation. */
        fputs("\"+Infinity\"", f);
        return;
    }
    if (v < 0 && v * 0.5 == v) {
        fputs("\"-Infinity\"", f);
        return;
    }
    /* %.17g — round-trip-safe for IEEE-754 double. Use a fixed buffer
     * so we don't depend on snprintf availability; 32 chars is ample
     * (the longest %.17g output is ~24 chars including sign, exponent,
     * and decimal point). */
    char buf[40];
    int n = snprintf(buf, sizeof buf, "%.17g", v);
    if (n < 0 || (size_t)n >= sizeof buf) {
        /* Should be unreachable for a finite double — but if snprintf
         * ever surprises us, fail loud rather than emit a truncated
         * literal that would mis-grade. */
        fprintf(stderr, "jl_kv_double: snprintf overflow on %g\n", v);
        exit(2);
    }
    /* Disambiguation from a decimal-integer string: the TS-side
     * decodeInputValue (eval/harness/value_codec.ts) treats any
     * "%d"-shaped string as a BigInt, which would mis-route a finite
     * double like 1.0 (printed as "1" by %.17g) into the bigint path.
     * To keep the wire format unambiguous, we ensure every finite
     * double's emitted string contains either '.' or 'e'/'E'. If %.17g
     * produced a bare integer (no exponent, no decimal), append ".0"
     * so it cannot be confused with an integer literal.
     *
     * Note: for the signed-zero distinction, "%.17g" emits "0" for +0
     * and "-0" for -0; appending ".0" gives "0.0" and "-0.0", both of
     * which round-trip through JS `Number()` preserving the sign
     * (Object.is(Number("-0.0"), -0) === true in V8/Bun). */
    int needs_decimal = 1;
    for (int i = 0; i < n; i++) {
        if (buf[i] == '.' || buf[i] == 'e' || buf[i] == 'E') {
            needs_decimal = 0;
            break;
        }
    }
    if (needs_decimal) {
        /* Append ".0" — guaranteed to fit since buf is 40 bytes and
         * %.17g output is at most ~24 chars + sign. */
        if ((size_t)(n + 3) >= sizeof buf) {
            fprintf(stderr, "jl_kv_double: buffer too small to append .0\n");
            exit(2);
        }
        buf[n] = '.';
        buf[n + 1] = '0';
        buf[n + 2] = '\0';
    }
    fputc('"', f);
    fputs(buf, f);
    fputc('"', f);
}

/* Emit ,"key":"<escaped>". Escapes only `"` and `\` — sufficient for
 * the controlled string values goldens carry (mostly short tags); we
 * are not a general JSON library. */
static inline void jl_kv_str(FILE *f, int first, const char *key, const char *s) {
    jl__key(f, first, key);
    fputc('"', f);
    for (const char *p = s; *p; ++p) {
        if (*p == '"' || *p == '\\') fputc('\\', f);
        fputc(*p, f);
    }
    fputc('"', f);
}

/* Emit ,"key":"RNDN" (or RNDZ/RNDU/RNDD/RNDA). The string forms match
 * src/core.ts's RoundingMode enum verbatim. MPFR_RNDF (faithful) and
 * MPFR_RNDNA (retired) are deliberately rejected — the TS port does
 * not implement them and a silent fallthrough would create a
 * grader-invisible divergence.
 *
 * Ref: /usr/include/mpfr.h L103–L109 — mpfr_rnd_t enum.
 * Ref: src/core.ts L151 — RoundingMode definition. */
static inline void jl_kv_rnd(FILE *f, int first, const char *key, mpfr_rnd_t r) {
    const char *name;
    switch (r) {
        case MPFR_RNDN: name = "RNDN"; break;
        case MPFR_RNDZ: name = "RNDZ"; break;
        case MPFR_RNDU: name = "RNDU"; break;
        case MPFR_RNDD: name = "RNDD"; break;
        case MPFR_RNDA: name = "RNDA"; break;
        default:
            fprintf(stderr, "jl_kv_rnd: unsupported rounding mode %d\n", (int)r);
            exit(2);
    }
    jl__key(f, first, key);
    fprintf(f, "\"%s\"", name);
}

/* ------------------------------------------------------------------ */
/* F. Limb-array helper                                               */
/* ------------------------------------------------------------------ */

/* Emit ,"key":["<dec>","<dec>",...] — a GMP mpn limb array as an
 * array of decimal-string limbs, in **little-endian limb order**
 * (limbs[0] is the least-significant 64-bit word). Matches GMP's
 * internal convention exactly; the TS side reassembles via
 *
 *   let v = 0n;
 *   for (let i = a.length - 1; i >= 0; i--) v = (v << 64n) | BigInt(a[i]);
 *
 * Decimal-string form (not hex) because BigInt(s) parses decimal
 * natively without leading "0x" gymnastics.
 *
 * Ref: GMP manual §8.3 ("Low-level Functions") — "The least
 *   significant limb is stored at the lowest address (i.e. limbs[0])."
 *   This is the iron rule for every mpn_* port; agents trained on
 *   "first element = most significant" produce silently reversed
 *   results.
 * Ref: /usr/include/x86_64-linux-gnu/gmp.h L44–L47 — GMP_LIMB_BITS,
 *   GMP_NUMB_BITS (NAIL_BITS == 0 on x86_64, so they coincide). */
static inline void jl_kv_limbs(FILE *f, int first, const char *key,
                               const mp_limb_t *limbs, size_t n) {
    jl__key(f, first, key);
    fputc('[', f);
    for (size_t i = 0; i < n; ++i) {
        if (i) fputc(',', f);
        fprintf(f, "\"%" PRIu64 "\"", (uint64_t)limbs[i]);
    }
    fputc(']', f);
}

/* ------------------------------------------------------------------ */
/* G. MPFR value helper — the load-bearing one                        */
/* ------------------------------------------------------------------ */

/* Emit ,"key":{...} where the inner object is the locked MPFR shape
 * from src/core.ts:
 *
 *     { kind: "normal"|"zero"|"inf"|"nan",
 *       sign: 1|-1,
 *       prec: "<decimal bigint>",
 *       exp:  "<decimal bigint>",
 *       mant: "<decimal bigint>" }
 *
 * Singular cases map per src/core.ts §"validate":
 *
 *   * nan  → { kind:"nan", sign:1, prec:"0", exp:"0", mant:"0" }
 *     The prec=0 + sign=1 sentinel is a TS-side convention because
 *     no NaN-producing op preserves the originating precision.
 *   * inf  → { kind:"inf",  sign:±1, prec:"<p>", exp:"0", mant:"0" }
 *   * zero → { kind:"zero", sign:±1, prec:"<p>", exp:"0", mant:"0" }
 *
 * For normal values we use mpfr_get_z_2exp, which returns
 *
 *     f = sign * |z| * 2^(MPFR_GET_EXP(f) - MPFR_PREC(f))
 *
 * with |z| already right-shifted by `sh = (-prec) mod GMP_NUMB_BITS`
 * (see mpfr/src/get_z_2exp.c L73–L75) so the limb-padding zeros are
 * stripped and |z| has exactly `prec` significant bits — i.e. it's
 * MSB-aligned to `prec` bits with no further work.
 *
 * The TS schema (src/core.ts L51–L62) writes the same value as
 *
 *     sign * mant * 2^(exp - prec)
 *
 * so the round-trip is simply
 *
 *     mant = |z|
 *     exp  = MPFR_GET_EXP(f)              (the returned exp_2 + prec)
 *     prec = MPFR_PREC(f)
 *     sign = MPFR_SIGN(f) > 0 ? 1 : -1
 *
 * No further bit-shift adjustment is required — `mpfr_get_z_2exp`
 * has done all the alignment work for us. (Earlier rev of this code
 * tried Option B, mpfr_get_str base 16, and lost MSB normalisation
 * on subnormal-ish values; Option A is both correct and one-liner.)
 *
 * Ref: /usr/include/mpfr.h L247–L256 — _sign*(_d[k-1]/B+...)·2^_exp
 *   value formula and "_d[k-1] >= B/2" MSB normalisation rule.
 * Ref: mpfr/src/get_z_2exp.c L50–L95 — implementation of
 *   mpfr_get_z_2exp, in particular the mpn_rshift by sh on L73–L75
 *   which is what makes Option A "free".
 * Ref: mpfr/src/mpfr.h L266–L271 — same storage convention
 *   documented in the upstream header. */
static inline void jl_kv_mpfr(FILE *f, int first, const char *key, mpfr_srcptr x) {
    jl__key(f, first, key);

    if (mpfr_nan_p(x)) {
        /* TS-side NaN sentinel: prec=0, sign=1, exp=0, mant=0. */
        fputs("{\"kind\":\"nan\",\"sign\":1,\"prec\":\"0\",\"exp\":\"0\",\"mant\":\"0\"}", f);
        return;
    }

    const int sign = (MPFR_SIGN(x) < 0) ? -1 : 1;
    /* mpfr_get_prec returns mpfr_prec_t (signed long-ish); positive
     * for any non-NaN value. We emit as int64 since prec_t may be 32-
     * or 64-bit depending on _MPFR_PREC_FORMAT. */
    const int64_t prec_i = (int64_t)mpfr_get_prec(x);

    if (mpfr_inf_p(x)) {
        fprintf(f,
                "{\"kind\":\"inf\",\"sign\":%d,\"prec\":\"%" PRId64 "\",\"exp\":\"0\",\"mant\":\"0\"}",
                sign, prec_i);
        return;
    }

    if (mpfr_zero_p(x)) {
        fprintf(f,
                "{\"kind\":\"zero\",\"sign\":%d,\"prec\":\"%" PRId64 "\",\"exp\":\"0\",\"mant\":\"0\"}",
                sign, prec_i);
        return;
    }

    /* Normal (regular) value. */
    mpz_t z;
    mpz_init(z);
    const mpfr_exp_t exp_2 = mpfr_get_z_2exp(z, x);
    /* z is signed; |z| is the MSB-aligned mantissa. exp_2 satisfies
     * f = z * 2^exp_2, and exp_ts (the schema's exp) = exp_2 + prec. */
    mpz_abs(z, z);
    char *mant_str = mpz_get_str(NULL, 10, z);  /* malloc'd by GMP */
    const int64_t exp_ts = (int64_t)exp_2 + prec_i;

    fprintf(f,
            "{\"kind\":\"normal\",\"sign\":%d,\"prec\":\"%" PRId64 "\","
            "\"exp\":\"%" PRId64 "\",\"mant\":\"%s\"}",
            sign, prec_i, exp_ts, mant_str);

    /* mpz_get_str(NULL, ...) allocates via the GMP allocator; free
     * through the matching deallocator to be hygienic on systems
     * where they differ. */
    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(mant_str, strlen(mant_str) + 1);
    mpz_clear(z);
}

/* ------------------------------------------------------------------ */
/* H. Output-object helpers                                           */
/* ------------------------------------------------------------------ */

/* Begin a struct-shaped output: writes ,"output":{ — callers then
 * use jl_kv_* with `first=1` on the first key. Pair with
 * jl_output_end_object. */
static inline void jl_output_begin_object(FILE *f) {
    fputs(",\"output\":{", f);
}

/* Close the output object opened by jl_output_begin_object. */
static inline void jl_output_end_object(FILE *f) { fputs("}", f); }

/* Emit ,"output":"<decimal>" — scalar uint64 result as decimal
 * string. Use for ops whose return type is a single mp_limb_t etc. */
static inline void jl_output_scalar_u64(FILE *f, uint64_t v) {
    fprintf(f, ",\"output\":\"%" PRIu64 "\"", v);
}

/* Emit ,"output":"<decimal>" — scalar int64 result as decimal
 * string (signed; negative values get the minus sign). Distinct from
 * `jl_output_scalar_u64` so callers don't accidentally widen a signed
 * negative into the unsigned 2's-complement form. The TS-side
 * `decodeExpectedOutput` matches the decimal-integer string regex
 * (`/^-?\d+$/`) and decodes to `{kind:'scalar', value:<bigint>}`, then
 * `compareOutput`'s bigint branch (value_codec.ts L429–L446) accepts a
 * bigint actual or coerces an integer JS number — so a port returning
 * a JS `bigint` like `-9223372036854775808n` grades correctly against
 * a `"-9223372036854775808"` golden line. Use for ops whose return
 * type is a signed C `long` (mpfr_get_si etc.). */
static inline void jl_output_scalar_i64(FILE *f, int64_t v) {
    fprintf(f, ",\"output\":\"%" PRId64 "\"", v);
}

/* Emit ,"output":N — scalar int as a bare JS number, NOT stringified.
 * Use for ops whose return type is a small signed int that comfortably
 * fits in a JS number — comparison ops (sign of difference: -1/0/+1),
 * ternary flags returned standalone, predicate "boolean" returns
 * encoded as int. The TS-side `decodeExpectedOutput` recognises a
 * bare-number `output` field as `{kind:'scalar', value:<number>}`
 * (see eval/harness/value_codec.ts L250–L252) and `compareOutput`
 * for that variant uses strict `===` against the port's return value,
 * so the port must return a plain `number` (NOT a bigint). Distinct
 * from `jl_output_scalar_u64` so callers don't accidentally widen a
 * signed -1 into the unsigned UINT64_MAX. */
static inline void jl_output_scalar_int(FILE *f, int v) {
    fprintf(f, ",\"output\":%d", v);
}

/* Emit ,"output":"<token>" where <token> follows the same convention as
 * jl_kv_double: a quoted "%.17g" lossless decimal for finite doubles
 * (with a ".0" suffix appended when the format produced a bare integer,
 * so the wire is unambiguous against the bigint path), and the quoted
 * sentinel tokens "NaN" / "+Infinity" / "-Infinity" for the IEEE 754
 * specials. Use for the bare-double return type of ops like mpfr_get_d.
 *
 * Why a quoted string and not a bare JSON number? Because JSON cannot
 * encode NaN or ±Infinity as bare numbers (RFC 8259), and emitting a
 * bare `NaN` produces a parse error in standards-compliant decoders.
 * Quoting every double — finite or not — keeps the wire format uniform
 * so the TS-side decoder needs a single rule.
 *
 * The TS-side decodeExpectedOutput in eval/harness/value_codec.ts
 * recognises the three sentinel tokens and finite-double-shaped strings
 * (matched by /^-?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?$/) and decodes
 * to {kind: 'scalar', value: <number>}. The compareOutput's scalar
 * branch uses Object.is(actual, expected) on numbers to handle the
 * NaN-equality and +0/-0 distinction correctly.
 *
 * Ref: jl_kv_double for the input-side counterpart and the rationale
 *   behind the ".0" disambiguation.
 * Ref: eval/harness/value_codec.ts §decodeExpectedOutput — sentinel +
 *   finite-double parsing.
 * Ref: eval/harness/value_codec.ts §compareOutput "scalar" branch —
 *   Object.is for the number-vs-number equality. */
static inline void jl_output_scalar_double(FILE *f, double v) {
    if (v != v) {
        fputs(",\"output\":\"NaN\"", f);
        return;
    }
    if (v > 0 && v * 0.5 == v) {
        fputs(",\"output\":\"+Infinity\"", f);
        return;
    }
    if (v < 0 && v * 0.5 == v) {
        fputs(",\"output\":\"-Infinity\"", f);
        return;
    }
    char buf[40];
    int n = snprintf(buf, sizeof buf, "%.17g", v);
    if (n < 0 || (size_t)n >= sizeof buf) {
        fprintf(stderr, "jl_output_scalar_double: snprintf overflow on %g\n", v);
        exit(2);
    }
    int needs_decimal = 1;
    for (int i = 0; i < n; i++) {
        if (buf[i] == '.' || buf[i] == 'e' || buf[i] == 'E') {
            needs_decimal = 0;
            break;
        }
    }
    if (needs_decimal) {
        if ((size_t)(n + 3) >= sizeof buf) {
            fprintf(stderr, "jl_output_scalar_double: buffer too small to append .0\n");
            exit(2);
        }
        buf[n] = '.';
        buf[n + 1] = '0';
        buf[n + 2] = '\0';
    }
    fputs(",\"output\":\"", f);
    fputs(buf, f);
    fputc('"', f);
}

/* Emit ,"output":"<escaped>" — scalar string output. Same simple
 * escape policy as jl_kv_str. */
static inline void jl_output_scalar_str(FILE *f, const char *s) {
    fputs(",\"output\":\"", f);
    for (const char *p = s; *p; ++p) {
        if (*p == '"' || *p == '\\') fputc('\\', f);
        fputc(*p, f);
    }
    fputc('"', f);
}

/* Emit ,"output":{...mpfr...} — the bare MPFR shape under the
 * "output" key. Use when the function returns just an MPFR value
 * with no ternary (e.g. constructors, conversions that don't round).
 * For the canonical (value, ternary) pair use jl_output_result. */
static inline void jl_output_mpfr(FILE *f, mpfr_srcptr x) {
    /* Reuse jl_kv_mpfr with first=1 by emitting our own ,"output": prefix. */
    fputs(",", f);
    jl_kv_mpfr(f, 1, "output", x);
}

/* Emit ,"output":{"value":{...mpfr...},"ternary":<-1|0|1>} — the
 * canonical Result shape from src/core.ts L173–L176. Used by every
 * rounding op (add/mul/sqrt/...). */
static inline void jl_output_result(FILE *f, mpfr_srcptr value, int ternary) {
    fputs(",\"output\":{", f);
    jl_kv_mpfr(f, 1, "value", value);
    /* MPFR returns the ternary as int; normalise to -1/0/+1 because
     * MPFR's contract is "sign of (rounded - exact)" not the raw int. */
    const int t = (ternary > 0) ? 1 : (ternary < 0) ? -1 : 0;
    jl_kv_int(f, 0, "ternary", t);
    fputs("}", f);
}

#endif /* MPFR_TS_GOLDEN_MASTER_COMMON_H */
