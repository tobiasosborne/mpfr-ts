"""Tests for eval/driver/callgraph.py — the mpfr_*/mpn_* call-graph extractor.

Strict-TDD: every test here was written BEFORE callgraph.py existed; the
suite is the executable spec.

Test layout:

  1. Smallest fixture — two synthetic .c files, three functions, one
     cross-file dependency. Verifies extraction, dedup, sort, and topo
     order.
  2. Self-edge filtering — `mpfr_x` calling `mpfr_x` is not a dependency
     on itself.
  3. Duplicate callsites collapse — multiple calls to the same callee in
     the same body produce one dep entry.
  4. Class detection — file path + function name drive the class bucket
     (substrate / transcendental / conversion / arithmetic / misc).
  5. Cycle handling — A->B->A produces both functions with topo ranks
     and a `cycle: true` flag.
  6. Real-world smoke — point at the actual mpfr/src/ and assert
     `mpfr_add` exists with arithmetic class and known callee. Skipped
     if the real source tree is missing or the run is too slow.

Run with:

    /home/tobias/.local/bin/pytest \
        /home/tobias/Projects/mpfr-ts/eval/driver/tests/test_callgraph.py -v
"""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

import pytest

# Make eval/driver/ importable as a package-less module set.
DRIVER_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(DRIVER_DIR))

# This import will fail in the RED phase — that's the point.
import callgraph as cg  # noqa: E402


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def write_c(tmpdir: Path, name: str, body: str) -> Path:
    """Write a synthetic .c file under tmpdir/, return its path."""
    p = tmpdir / name
    p.write_text(body)
    return p


# ---------------------------------------------------------------------------
# 1. Smallest fixture
# ---------------------------------------------------------------------------


def test_smallest_fixture(tmp_path: Path) -> None:
    """Three functions across two files, with a chain of deps."""
    write_c(
        tmp_path,
        "foo.c",
        """
        void mpfr_foo(void) { mpfr_bar(); mpn_baz(); }
        """,
    )
    write_c(
        tmp_path,
        "bar.c",
        """
        void mpfr_bar(void) { mpn_baz(); }
        void mpn_baz(void) {}
        """,
    )

    graph = cg.walk_mpfr_src(tmp_path)

    assert set(graph.keys()) == {"mpfr_foo", "mpfr_bar", "mpn_baz"}
    assert graph["mpfr_foo"].deps == ["mpfr_bar", "mpn_baz"]
    assert graph["mpfr_bar"].deps == ["mpn_baz"]
    assert graph["mpn_baz"].deps == []

    # Topo order: mpn_baz (rank 0) < mpfr_bar (rank 1) < mpfr_foo (rank 2).
    assert graph["mpn_baz"].topo_rank < graph["mpfr_bar"].topo_rank
    assert graph["mpfr_bar"].topo_rank < graph["mpfr_foo"].topo_rank

    # Class assignment: mpn_* is substrate.
    assert graph["mpn_baz"].class_ == "substrate"

    # `defined_in` is a path relative to the source root.
    assert graph["mpfr_foo"].defined_in.endswith("foo.c")
    assert graph["mpfr_bar"].defined_in.endswith("bar.c")


# ---------------------------------------------------------------------------
# 2. Self-edge filtering
# ---------------------------------------------------------------------------


def test_self_edge_filtered(tmp_path: Path) -> None:
    """A function calling itself does not list itself in deps."""
    write_c(
        tmp_path,
        "x.c",
        """
        void mpfr_x(void) { mpfr_x(); mpfr_y(); }
        void mpfr_y(void) {}
        """,
    )

    graph = cg.walk_mpfr_src(tmp_path)
    assert graph["mpfr_x"].deps == ["mpfr_y"]


# ---------------------------------------------------------------------------
# 3. Duplicate callsites collapse
# ---------------------------------------------------------------------------


def test_duplicate_callsites_collapse(tmp_path: Path) -> None:
    """Multiple calls to the same callee dedup to one entry in deps."""
    write_c(
        tmp_path,
        "a.c",
        """
        void mpfr_a(void) { mpfr_b(); mpfr_b(); mpfr_c(); }
        void mpfr_b(void) {}
        void mpfr_c(void) {}
        """,
    )

    graph = cg.walk_mpfr_src(tmp_path)
    assert graph["mpfr_a"].deps == ["mpfr_b", "mpfr_c"]


# ---------------------------------------------------------------------------
# 4. Class detection
# ---------------------------------------------------------------------------


def test_class_transcendental(tmp_path: Path) -> None:
    """`mpfr_exp` in exp.c should be classified `transcendental`."""
    write_c(
        tmp_path,
        "exp.c",
        "void mpfr_exp(void) {}",
    )
    graph = cg.walk_mpfr_src(tmp_path)
    assert graph["mpfr_exp"].class_ == "transcendental"


def test_class_arithmetic(tmp_path: Path) -> None:
    """`mpfr_add` in add.c is `arithmetic`."""
    write_c(
        tmp_path,
        "add.c",
        "void mpfr_add(void) {}",
    )
    graph = cg.walk_mpfr_src(tmp_path)
    assert graph["mpfr_add"].class_ == "arithmetic"


def test_class_conversion(tmp_path: Path) -> None:
    """`mpfr_get_d` in get_d.c is `conversion`."""
    write_c(
        tmp_path,
        "get_d.c",
        "void mpfr_get_d(void) {}",
    )
    graph = cg.walk_mpfr_src(tmp_path)
    assert graph["mpfr_get_d"].class_ == "conversion"


def test_class_misc(tmp_path: Path) -> None:
    """A cache_init-style function lands in `misc`."""
    write_c(
        tmp_path,
        "cache.c",
        "void mpfr_cache_init(void) {}",
    )
    graph = cg.walk_mpfr_src(tmp_path)
    assert graph["mpfr_cache_init"].class_ == "misc"


def test_class_substrate(tmp_path: Path) -> None:
    """Any `mpn_*` function is `substrate`, regardless of file."""
    write_c(
        tmp_path,
        "weird.c",
        "void mpn_something_unusual(void) {}",
    )
    graph = cg.walk_mpfr_src(tmp_path)
    assert graph["mpn_something_unusual"].class_ == "substrate"


# ---------------------------------------------------------------------------
# 5. Cycle handling
# ---------------------------------------------------------------------------


def test_cycle_handling(tmp_path: Path) -> None:
    """A->B->A: both nodes present, ranked, flagged with cycle=True."""
    write_c(
        tmp_path,
        "ab.c",
        """
        void mpfr_a(void) { mpfr_b(); }
        void mpfr_b(void) { mpfr_a(); }
        """,
    )

    graph = cg.walk_mpfr_src(tmp_path)
    assert "mpfr_a" in graph
    assert "mpfr_b" in graph
    # Both should have a topo_rank (i.e. they're in the order somewhere).
    assert isinstance(graph["mpfr_a"].topo_rank, int)
    assert isinstance(graph["mpfr_b"].topo_rank, int)
    # Both should be flagged as participating in a cycle.
    assert graph["mpfr_a"].cycle is True
    assert graph["mpfr_b"].cycle is True


# ---------------------------------------------------------------------------
# Comment / string stripping
# ---------------------------------------------------------------------------


def test_call_inside_comment_ignored(tmp_path: Path) -> None:
    """A callsite that lives in a comment must NOT contribute to deps."""
    write_c(
        tmp_path,
        "c.c",
        """
        // void mpfr_a(void) { mpfr_ghost1(); }
        /* mpfr_ghost2(); */
        void mpfr_real(void) { mpfr_actual(); /* mpfr_ghost3(); */ }
        void mpfr_actual(void) {}
        """,
    )
    graph = cg.walk_mpfr_src(tmp_path)
    assert "mpfr_real" in graph
    assert graph["mpfr_real"].deps == ["mpfr_actual"]


def test_call_inside_string_ignored(tmp_path: Path) -> None:
    """A callsite that lives in a string literal must NOT contribute to deps."""
    write_c(
        tmp_path,
        "s.c",
        """
        void mpfr_real(void) {
          puts("mpfr_ghost()");
          mpfr_actual();
        }
        void mpfr_actual(void) {}
        """,
    )
    graph = cg.walk_mpfr_src(tmp_path)
    assert graph["mpfr_real"].deps == ["mpfr_actual"]


# ---------------------------------------------------------------------------
# JSON emission
# ---------------------------------------------------------------------------


def test_emit_json_schema(tmp_path: Path) -> None:
    """`emit_json` produces the documented schema."""
    write_c(
        tmp_path,
        "add.c",
        """
        void mpfr_add(void) { mpn_add_n(); }
        void mpn_add_n(void) {}
        """,
    )
    graph = cg.walk_mpfr_src(tmp_path)
    out = json.loads(cg.emit_json(graph, src_root=tmp_path))

    assert "generated_at" in out
    assert "mpfr_source_root" in out
    assert "functions" in out

    fns = out["functions"]
    assert set(fns.keys()) == {"mpfr_add", "mpn_add_n"}

    entry = fns["mpfr_add"]
    assert entry["deps"] == ["mpn_add_n"]
    assert entry["class"] == "arithmetic"
    assert isinstance(entry["topo_rank"], int)
    assert entry["defined_in"].endswith("add.c")


def test_emit_json_idempotent(tmp_path: Path) -> None:
    """Re-running emit_json on the same graph produces byte-identical output."""
    write_c(
        tmp_path,
        "x.c",
        """
        void mpfr_x(void) { mpfr_y(); mpfr_z(); }
        void mpfr_y(void) {}
        void mpfr_z(void) {}
        """,
    )
    graph = cg.walk_mpfr_src(tmp_path)
    # Strip the generated_at field which is naturally non-deterministic; the
    # rest of the document MUST be stable.
    a = json.loads(cg.emit_json(graph, src_root=tmp_path))
    b = json.loads(cg.emit_json(graph, src_root=tmp_path))
    a.pop("generated_at", None)
    b.pop("generated_at", None)
    assert a == b


# ---------------------------------------------------------------------------
# 6. Real-world smoke (skip if slow)
# ---------------------------------------------------------------------------


REAL_MPFR_SRC = Path("/home/tobias/Projects/mpfr-ts/mpfr/src")


@pytest.mark.skipif(
    not REAL_MPFR_SRC.is_dir(),
    reason="real mpfr/src/ not present",
)
def test_real_mpfr_src_smoke() -> None:
    """Cheap sanity check against the actual mpfr/src/ tree."""
    start = time.monotonic()
    graph = cg.walk_mpfr_src(REAL_MPFR_SRC)
    elapsed = time.monotonic() - start

    if elapsed > 5.0:
        pytest.skip(f"walk too slow for smoke test ({elapsed:.1f}s)")

    # Sanity: at least 100 functions, mpfr_add present, classified arithmetic.
    assert len(graph) > 100
    assert "mpfr_add" in graph
    assert graph["mpfr_add"].class_ == "arithmetic"

    # mpfr_add definitely calls at least one mpfr_* or mpn_* helper.
    assert len(graph["mpfr_add"].deps) > 0
