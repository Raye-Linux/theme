#!/usr/bin/env python3
"""Everything the package build's tests import must be in Build-Depends.

    python3 -m pytest -q tests/test_build_deps_cover_the_tests.py

🚨 Why. debian/rules broadened its test step to `pytest tests/ grub/tests/
art/` on 2026-11-06 (61cad14) -- correct, and the first change to put art/
inside the gate that decides whether the ISO ships. art/signin.py imports
PIL at module level, and python3-pil was not in Build-Depends, so the image
build died THIRTY-SIX MINUTES in:

    ModuleNotFoundError: No module named 'PIL'
    ERROR art/test_signin.py
    Interrupted: 1 error during collection

⚠️ And an interrupted collection runs NOTHING: zero of art/'s 326 checks,
not the thirteen of that one file. A missing test dependency is therefore
not a small loss, it is the whole folder.

🚨 The quiet half matters as much. art/test_sounds.py does
`pytest.importorskip("numpy")` at module level: without python3-numpy those
twelve checks SKIP, and the build stays GREEN having not run them. That is
precisely what 61cad14 was written to stop, one level down -- so a
module-level importorskip is held to the same rule as a plain import.

⚠️ An unmapped third-party module FAILS rather than being ignored. The
opposite default would make this file go quiet the first time a test picks
up a new dependency, which is the shape of the defect it exists for.
"""
import ast
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
RULES = REPO / "debian" / "rules"
CONTROL = REPO / "debian" / "control"
PYTEST_LINE = re.compile(r"^\t\s*python3 -m pytest\s+(.*)$", re.M)

# python module -> the Debian package that provides it. Deliberately a
# table and not a guess: `import PIL` comes from python3-pil, which no rule
# of thumb produces.
PROVIDED_BY = {
    "pytest": "python3-pytest",
    "PIL": "python3-pil",
    "numpy": "python3-numpy",
    "yaml": "python3-yaml",
    "pyparsing": "python3-pyparsing",
    "cairosvg": "python3-cairosvg",
    "lxml": "python3-lxml",
}


def no_comments(text):
    return "\n".join(l for l in text.splitlines()
                     if not l.lstrip().startswith("#"))


def pytest_paths():
    """The paths debian/rules hands to pytest, options dropped."""
    out = []
    for args in PYTEST_LINE.findall(no_comments(RULES.read_text())):
        for word in args.split():
            if word.startswith("-") or "=" in word:
                continue
            if word in ("python3", "pytest", "no:cacheprovider"):
                continue
            p = REPO / word
            if p.exists():
                out.append(p)
    return out


def build_depends():
    """The package names in Build-Depends, comments stripped."""
    clean = no_comments(CONTROL.read_text())
    m = re.search(r"^Build-Depends:(.*?)(?=^[A-Z][A-Za-z-]*:)", clean,
                  re.S | re.M)
    assert m, "debian/control has no Build-Depends field"
    names = set()
    for dep in m.group(1).replace("\n", " ").split(","):
        dep = dep.strip()
        if dep:
            names.add(dep.split()[0].split(":")[0])
    return names


def toplevel_imports(path):
    """Module-level imports of one file, plus module-level importorskip."""
    tree = ast.parse(path.read_text(), filename=str(path))
    found = set()
    for node in tree.body:                      # body only: module level
        if isinstance(node, ast.Import):
            found.update(a.name.split(".")[0] for a in node.names)
        elif isinstance(node, ast.ImportFrom) and node.level == 0 and node.module:
            found.add(node.module.split(".")[0])
        # np = pytest.importorskip("numpy") -- a skip the build would not see
        elif isinstance(node, ast.Assign) and isinstance(node.value, ast.Call):
            f = node.value.func
            if (isinstance(f, ast.Attribute) and f.attr == "importorskip"
                    and node.value.args
                    and isinstance(node.value.args[0], ast.Constant)):
                found.add(node.value.args[0].value)
    return found


SKIP_DIRS = {"__pycache__", ".git", "work", "out", ".pytest_cache",
             "third-party", "debian"}


def local_modules():
    """<module name> -> file, for every .py in the repo.

    ⚠️ A sibling lookup is not enough. tests/colors/test_contrast.py does
    `sys.path.insert(0, ROOT + "/tokens")` and then imports `accent` and
    `generate`, which live in tokens/ -- two directories away. Resolving
    only siblings made this file call them unknown third-party modules,
    which is a false red; resolving nothing would make it miss a real
    dependency reached the same way. Measured 2026-11-07 by running it.
    """
    index = {}
    for f in REPO.rglob("*.py"):
        if SKIP_DIRS & set(f.parts):
            continue
        index.setdefault(f.stem, f)
    return index


def reachable_files(paths):
    """Every test file under those paths, and the local modules they import.

    A test that imports a local module inherits ITS module-level imports:
    art/test_signin.py imports `signin`, and it is art/signin.py that
    imports PIL. Looking only at the test file would have missed the very
    case this file exists for.
    """
    index = local_modules()
    files, seen = [], set()
    for root in paths:
        candidates = sorted(root.rglob("test_*.py")) if root.is_dir() else [root]
        for f in candidates:
            if SKIP_DIRS & set(f.parts):
                continue
            stack = [f]
            while stack:
                cur = stack.pop()
                if cur in seen:
                    continue
                seen.add(cur)
                files.append(cur)
                for name in toplevel_imports(cur):
                    local = index.get(name)
                    if local is not None:
                        stack.append(local)
    return files


def test_every_module_the_tests_import_has_a_known_package():
    paths = pytest_paths()
    assert paths, "debian/rules hands pytest no path this test can resolve"
    index = local_modules()
    unknown = {}
    for f in reachable_files(paths):
        for mod in toplevel_imports(f):
            if mod in sys.stdlib_module_names or mod in PROVIDED_BY:
                continue
            if mod in index or (f.parent / mod).is_dir():
                continue
            unknown.setdefault(mod, []).append(str(f.relative_to(REPO)))
    assert not unknown, (
        "these modules are imported by the package build's tests and this "
        "file does not know which Debian package provides them, so it cannot "
        "check Build-Depends. Add them to PROVIDED_BY: "
        + "; ".join(f"{m} ({', '.join(v)})" for m, v in sorted(unknown.items())))


def test_build_depends_provides_them():
    paths = pytest_paths()
    assert paths, "debian/rules hands pytest no path this test can resolve"
    declared = build_depends()
    needed, where = {}, {}
    for f in reachable_files(paths):
        for mod in toplevel_imports(f):
            if mod in PROVIDED_BY:
                needed[mod] = PROVIDED_BY[mod]
                where.setdefault(mod, set()).add(str(f.relative_to(REPO)))
    # "one is not zero": a rename of debian/rules' pytest line, or of the
    # folders, would leave this with nothing to check -- and nothing to
    # check looks exactly like everything in order.
    assert len(needed) >= 2, (
        "this check found %d third-party module(s) in the package build's "
        "tests, which is too few to be true: art/ alone needs PIL and numpy. "
        "Either the pytest paths in debian/rules moved, or the import "
        "detection no longer matches them." % len(needed))
    missing = {m: p for m, p in needed.items() if p not in declared}
    assert not missing, "\n".join(
        "%s is imported by %s and %s is NOT in Build-Depends: without it "
        "pytest %s during the package build, which is the gate that decides "
        "whether the ISO ships"
        % (m, ", ".join(sorted(where[m])), p,
           "dies collecting the whole folder" if m != "numpy"
           else "skips those checks and stays green")
        for m, p in sorted(missing.items()))
