"""Tests for eval/driver/gen_spec.py. Strict-TDD: written before the module.

Run with: pytest eval/driver/tests/test_gen_spec.py -v
"""

from __future__ import annotations

import sys
from pathlib import Path

DRIVER_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(DRIVER_DIR))

import gen_spec  # noqa: E402


def _w(tmp: Path, name: str, body: str) -> Path:
    p = tmp / name
    p.write_text(body)
    return p


def test_add_d_typical_wrapper(tmp_path: Path) -> None:
    c = _w(tmp_path, "add_d.c",
           "int\nmpfr_add_d (mpfr_ptr a, mpfr_srcptr b, double c, mpfr_rnd_t rnd_mode)\n{ return 0; }\n")
    spec = gen_spec.extract_spec(c, "mpfr_add_d")
    assert spec["function"] == "mpfr_add_d"
    assert spec["class"] == "arithmetic"
    assert spec["signature"]["params"] == ["b", "c", "prec", "rnd"]
    assert spec["signature"]["returns"] == "Result"
    assert spec["c_signature"] == "int mpfr_add_d (mpfr_ptr a, mpfr_srcptr b, double c, mpfr_rnd_t rnd_mode)"
    assert spec["prec_unit"] == "bits"
    assert spec["doc"] == "TODO: opus fills this in"
    assert spec["divergence_from_c"] == "TODO: opus fills this in"
    assert len(spec["refs"]) >= 3
    assert any("add_d.c" in r and "C reference body" in r for r in spec["refs"])
    assert any("src/core.ts" in r for r in spec["refs"])
    assert any("CLAUDE.md" in r for r in spec["refs"])


def test_cmp_no_rop_int_return(tmp_path: Path) -> None:
    """int mpfr_cmp(...) -> number; the static mpfr_cmp3 must be skipped."""
    c = _w(tmp_path, "cmp.c",
           "static int\nmpfr_cmp3 (mpfr_srcptr b, mpfr_srcptr c, int s) { return 0; }\n"
           "int\nmpfr_cmp (mpfr_srcptr b, mpfr_srcptr c) { return mpfr_cmp3 (b, c, 1); }\n")
    spec = gen_spec.extract_spec(c, "mpfr_cmp")
    assert spec["signature"]["returns"] == "number"
    assert spec["signature"]["params"] == ["b", "c"]
    assert spec["class"] == "misc"
    assert "static" not in spec["c_signature"]
    assert spec["c_signature"] == "int mpfr_cmp (mpfr_srcptr b, mpfr_srcptr c)"


def test_init2_void_return(tmp_path: Path) -> None:
    c = _w(tmp_path, "init2.c", "void\nmpfr_init2 (mpfr_ptr x, mpfr_prec_t p)\n{ }\n")
    spec = gen_spec.extract_spec(c, "mpfr_init2")
    assert spec["signature"]["returns"].startswith("TODO:")
    assert "void" in spec["signature"]["returns"]
    assert "prec" in spec["signature"]["params"]
    assert "rnd" not in spec["signature"]["params"]
    assert spec["class"] == "misc"


def test_sqr_class_inference(tmp_path: Path) -> None:
    """sqr*.c filename -> arithmetic; rop slot dropped -> prec appended."""
    c = _w(tmp_path, "sqr.c", "int\nmpfr_sqr (mpfr_ptr a, mpfr_srcptr b, mpfr_rnd_t rnd_mode) { return 0; }\n")
    spec = gen_spec.extract_spec(c, "mpfr_sqr")
    assert spec["class"] == "arithmetic"
    assert spec["signature"]["returns"] == "Result"
    assert spec["signature"]["params"] == ["b", "prec", "rnd"]


def test_get_str_multiline_complex_sig(tmp_path: Path) -> None:
    """Multi-line decl collapsed; unknown types -> TODO; rnd appended."""
    c = _w(tmp_path, "get_str.c",
           "static int mpfr_get_str_aux (char *const, mpfr_exp_t *const, mp_limb_t *const,\n"
           "    mp_size_t, mpfr_exp_t, mpfr_exp_t, int, size_t, mpfr_rnd_t);\n"
           "char *\nmpfr_get_str (char *s, mpfr_exp_t *e, int b, size_t m, mpfr_srcptr x,\n"
           "              mpfr_rnd_t rnd) { return s; }\n")
    spec = gen_spec.extract_spec(c, "mpfr_get_str")
    assert "\n" not in spec["c_signature"]
    assert "mpfr_get_str (char *s, mpfr_exp_t *e, int b, size_t m, mpfr_srcptr x, mpfr_rnd_t rnd)" in spec["c_signature"]
    assert spec["signature"]["returns"].startswith("TODO:")
    assert "rnd" in spec["signature"]["params"]
    assert "prec" not in spec["signature"]["params"]
    assert any(p.startswith("TODO:") for p in spec["signature"]["params"])


def test_class_hint_overrides_inference(tmp_path: Path) -> None:
    c = _w(tmp_path, "add_d.c",
           "int mpfr_add_d (mpfr_ptr a, mpfr_srcptr b, double c, mpfr_rnd_t rnd_mode) { return 0; }\n")
    spec = gen_spec.extract_spec(c, "mpfr_add_d", class_hint="transcendental")
    assert spec["class"] == "transcendental"


def test_refs_use_repo_relative_path(tmp_path: Path) -> None:
    """C ref uses mpfr/src/<basename>, not the tmp path."""
    c = _w(tmp_path, "add_d.c",
           "int mpfr_add_d (mpfr_ptr a, mpfr_srcptr b, double c, mpfr_rnd_t rnd_mode) { return 0; }\n")
    spec = gen_spec.extract_spec(c, "mpfr_add_d")
    c_ref = next(r for r in spec["refs"] if "C reference body" in r)
    assert "mpfr/src/add_d.c" in c_ref
    assert str(tmp_path) not in c_ref


def test_mpn_path_is_substrate(tmp_path: Path) -> None:
    sub = tmp_path / "mpn"
    sub.mkdir()
    c = _w(sub, "add_n.c", "void mpn_add_n (mp_ptr r, mp_srcptr a, mp_srcptr b, mp_size_t n) { }\n")
    spec = gen_spec.extract_spec(c, "mpn_add_n")
    assert spec["class"] == "substrate"
