"""Extract spec.json scaffolding from an MPFR C source.

`extract_spec(path, name, class_hint=None)` returns the dict the ralph
loop hands a porter agent. Strategy: strip comments+#directives, flatten
whitespace, walk back from `<name>(` to the prior `;`/`}` for the decl
head. Static decls are skipped. Unknown C types emit "TODO:" not guesses.
"""

from __future__ import annotations

import pathlib
import re
from typing import Any

_ARITH = ("add", "sub", "mul", "div", "sqr", "sqrt")
_TRANS = ("exp", "log", "sin", "cos", "tan", "asin", "acos",
          "atan", "sinh", "cosh", "tanh")
_KNOWN_TYPES = {"mpfr_srcptr", "double", "long", "int", "unsigned long",
                "unsigned int", "mpz_srcptr", "mpq_srcptr", "mpf_srcptr",
                "float", "long double", "size_t"}
_SKIP_ATTR = re.compile(
    r"\b(MPFR_HOT_FUNCTION_ATTR|MPFR_COLD_FUNCTION_ATTR|__attribute__\s*\(\([^)]*\)\))\s*"
)


def _infer_class(path: pathlib.Path) -> str:
    if "mpn" in [p.lower() for p in path.parts]:
        return "substrate"
    stem = path.stem.lower()
    if any(stem == s or stem.startswith(s) for s in _ARITH):
        return "arithmetic"
    if any(stem == s or stem.startswith(s) for s in _TRANS):
        return "transcendental"
    return "misc"


def _strip_noise(src: str) -> str:
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    src = re.sub(r"//[^\n]*", " ", src)
    return re.sub(r"^\s*#[^\n]*", " ", src, flags=re.M)


def _find_signature(src: str, name: str) -> str | None:
    flat = re.sub(r"\s+", " ", _strip_noise(src)).strip()
    pat = re.compile(r"\b" + re.escape(name) + r"\s*\(")
    for m in pat.finditer(flat):
        start = max(flat.rfind(";", 0, m.start()), flat.rfind("}", 0, m.start())) + 1
        head = _SKIP_ATTR.sub("", flat[start:m.start()].strip()).strip()
        if not head or head.startswith("static "):
            continue
        depth, i = 0, m.end() - 1
        while i < len(flat):
            if flat[i] == "(":
                depth += 1
            elif flat[i] == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        if depth != 0:
            continue
        return re.sub(r"\s+", " ",
                      f"{head} {name} ({flat[m.end():i].strip()})").strip()
    return None


def _return_type(decl: str, name: str) -> str:
    idx = decl.find(" " + name + " ")
    return decl[:idx].strip() if idx >= 0 else ""


def _map_param(raw: str) -> str | None:
    """One C param -> TS name, or TODO. None means caller-handled (rop/prec/rnd)."""
    toks = re.sub(r"\bconst\b", "", raw).strip().split()
    if not toks:
        return f"TODO: {raw}"
    type_str = " ".join(toks[:-1]) if len(toks) > 1 else toks[0]
    name = toks[-1].lstrip("*") if len(toks) > 1 else ""
    type_norm = type_str.replace("*", "").strip()
    if type_norm in _KNOWN_TYPES:
        return name or f"TODO: {raw}"
    if type_norm in {"mpfr_prec_t", "mpfr_rnd_t", "mpfr_ptr"}:
        return None
    return f"TODO: {raw}"


def _build_params(decl: str) -> tuple[list[str], bool]:
    inner = decl[decl.index("(") + 1: decl.rindex(")")].strip()
    raws = [] if (not inner or inner == "void") else [p.strip() for p in inner.split(",")]
    has_prec_t = any("mpfr_prec_t" in r for r in raws)
    has_rnd = any("mpfr_rnd_t" in r for r in raws)
    params, dropped = [], False
    for r in raws:
        clean = re.sub(r"\bconst\b", "", r).strip().split()
        type_norm = re.sub(r"\*", "", " ".join(clean[:-1]) if len(clean) > 1 else "").strip()
        if not dropped and type_norm == "mpfr_ptr":
            dropped = True
            continue
        m = _map_param(r)
        if m is not None:
            params.append(m)
    if has_prec_t or dropped:
        params.append("prec")
    if has_rnd:
        params.append("rnd")
    return params, dropped


def _classify_return(ret: str, dropped_first_ptr: bool) -> str:
    r = re.sub(r"\s+", " ", ret).strip()
    if r == "int":
        return "Result" if dropped_first_ptr else "number"
    if r == "mpfr_ptr":
        return "MPFR"
    return f"TODO: {r}" if r else "TODO: (empty)"


def _ref_path(p: pathlib.Path) -> str:
    parts = p.parts
    for i in range(len(parts) - 1, -1, -1):
        if parts[i] == "src" and i > 0 and parts[i - 1] == "mpfr":
            return "/".join(parts[i - 1:])
    return f"mpfr/src/{p.name}"


def extract_spec(
    c_source_path: pathlib.Path,
    function_name: str,
    class_hint: str | None = None,
) -> dict[str, Any]:
    decl = _find_signature(c_source_path.read_text(), function_name)
    if decl is None:
        raise ValueError(
            f"Could not locate non-static declaration of {function_name} in {c_source_path}"
        )
    params, dropped = _build_params(decl)
    returns = _classify_return(_return_type(decl, function_name), dropped)
    cls = class_hint or _infer_class(c_source_path)
    return {
        "function": function_name,
        "class": cls,
        "signature": {"params": params, "returns": returns},
        "c_signature": decl,
        "prec_unit": "bits",
        "doc": "TODO: opus fills this in",
        "divergence_from_c": "TODO: opus fills this in",
        "refs": [
            f"{_ref_path(c_source_path)} -- C reference body.",
            "src/core.ts -- value model and Result return shape.",
            "CLAUDE.md hallucination callouts -- NaN, signed zero, ternary direction.",
        ],
    }
