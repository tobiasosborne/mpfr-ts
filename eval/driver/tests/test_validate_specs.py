"""Tests for eval/driver/validate_specs.py. Run: pytest eval/driver/tests/test_validate_specs.py -v"""

from __future__ import annotations

import json
import sys
from pathlib import Path

DRIVER_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(DRIVER_DIR))

import validate_specs  # noqa: E402

REPO_ROOT = DRIVER_DIR.parent.parent


def _spec(tmp: Path, fn: str, body: dict) -> Path:
    (tmp / fn).mkdir()
    p = tmp / fn / "spec.json"
    p.write_text(json.dumps(body))
    return p


_MIN = {"function": "f", "class": "misc", "signature": {"params": [], "returns": "void"},
        "c_signature": "void f(void)", "prec_unit": "bits"}


def test_normalize_signature_whitespace_collapse() -> None:
    ns = validate_specs._normalize_signature
    assert ns("int  mpfr_x(  a,   b  )") == "int mpfr_x( a, b )"
    assert ns("  int\nmpfr_x\t(a,b)\n") == "int mpfr_x (a,b)"


def test_validate_spec_happy_path(tmp_path: Path) -> None:
    c = tmp_path / "add_d.c"
    c.write_text("int mpfr_add_d (mpfr_ptr a, mpfr_srcptr b, double c, mpfr_rnd_t rnd_mode) { return 0; }\n")
    curated = _spec(tmp_path, "mpfr_add_d", {
        "function": "mpfr_add_d", "class": "arithmetic",
        "signature": {"params": ["b", "c", "prec", "rnd"], "returns": "Result"},
        "c_signature": "int   mpfr_add_d (mpfr_ptr a, mpfr_srcptr b, double c, mpfr_rnd_t rnd_mode)",
        "prec_unit": "bits"})
    r = validate_specs.validate_spec(c, "mpfr_add_d", curated)
    assert r.extraction_error is None
    assert len(r.diffs) == 5 and all(d.matches for d in r.diffs)


def test_validate_spec_extraction_error(tmp_path: Path) -> None:
    c = tmp_path / "empty.c"; c.write_text("/* nothing */\n")
    curated = _spec(tmp_path, "mpfr_missing", {**_MIN, "function": "mpfr_missing"})
    r = validate_specs.validate_spec(c, "mpfr_missing", curated)
    assert r.extraction_error is not None and "mpfr_missing" in r.extraction_error
    assert r.diffs == []


def test_validate_spec_field_mismatch(tmp_path: Path) -> None:
    c = tmp_path / "cmp.c"; c.write_text("int mpfr_cmp (mpfr_srcptr b, mpfr_srcptr c) { return 0; }\n")
    curated = _spec(tmp_path, "mpfr_cmp", {
        "function": "mpfr_cmp", "class": "arithmetic",
        "signature": {"params": ["b", "c"], "returns": "void"},
        "c_signature": "int mpfr_cmp (mpfr_srcptr b, mpfr_srcptr c)", "prec_unit": "bits"})
    by = {d.field: d for d in validate_specs.validate_spec(c, "mpfr_cmp", curated).diffs}
    assert by["class"].gen_spec_value == "misc" and by["class"].curated_value == "arithmetic"
    assert by["class"].matches is False and by["signature.returns"].matches is False
    assert by["signature.params"].matches and by["c_signature"].matches and by["prec_unit"].matches


def test_validate_spec_real_add_d_no_diffs() -> None:
    c = REPO_ROOT / "mpfr" / "src" / "add_d.c"
    spec = REPO_ROOT / "eval" / "functions" / "mpfr_add_d" / "spec.json"
    if not c.exists() or not spec.exists():
        return
    r = validate_specs.validate_spec(c, "mpfr_add_d", spec)
    assert r.extraction_error is None
    by = {d.field: d for d in r.diffs}
    assert by["signature.params"].matches and by["signature.returns"].matches and by["prec_unit"].matches


def test_validate_spec_real_cmp_detects_class_diff() -> None:
    c = REPO_ROOT / "mpfr" / "src" / "cmp.c"
    spec = REPO_ROOT / "eval" / "functions" / "mpfr_cmp" / "spec.json"
    if not c.exists() or not spec.exists():
        return
    r = validate_specs.validate_spec(c, "mpfr_cmp", spec)
    assert r.extraction_error is None
    cd = next(d for d in r.diffs if d.field == "class")
    assert not cd.matches and cd.gen_spec_value == "misc" and cd.curated_value == "arithmetic"


def test_validate_all_specs_mocked(tmp_path: Path) -> None:
    eval_fns = tmp_path / "ef"; eval_fns.mkdir()
    mpfr_src = tmp_path / "ms"; mpfr_src.mkdir()
    (mpfr_src / "add_d.c").write_text(
        "int mpfr_add_d (mpfr_ptr a, mpfr_srcptr b, double c, mpfr_rnd_t rnd_mode) { return 0; }\n")
    _spec(eval_fns, "mpfr_add_d", {
        "function": "mpfr_add_d", "class": "arithmetic",
        "signature": {"params": ["b", "c", "prec", "rnd"], "returns": "Result"},
        "c_signature": "int mpfr_add_d (mpfr_ptr a, mpfr_srcptr b, double c, mpfr_rnd_t rnd_mode)",
        "prec_unit": "bits"})
    _spec(eval_fns, "mpfr_ghost", {**_MIN, "function": "mpfr_ghost"})
    _spec(eval_fns, "mpfr_unknown", {**_MIN, "function": "mpfr_unknown"})
    cg = tmp_path / "cg.json"
    cg.write_text(json.dumps({"functions": {
        "mpfr_add_d": {"defined_in": "add_d.c"},
        "mpfr_ghost": {"defined_in": "missing.c"}}}))
    by = {r.function: r for r in validate_specs.validate_all_specs(eval_fns, mpfr_src, cg)}
    assert "mpfr_unknown" not in by
    assert by["mpfr_add_d"].extraction_error is None
    assert by["mpfr_ghost"].extraction_error is not None and by["mpfr_ghost"].c_source is None
