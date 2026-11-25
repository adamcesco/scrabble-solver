"""Check the compiler's include graph for dependencies on another solver."""
import os
import shlex
import subprocess
import sys
from pathlib import Path

IMPLEMENTATIONS = Path("src/solvers")
INTERFACES = Path("include/solvers")


def solver_owner(path):
    if IMPLEMENTATIONS in path.parents:
        return path.relative_to(IMPLEMENTATIONS).parts[0]
    if path.parent == INTERFACES:
        return path.stem
    return None


def main(compiler):
    sources = sorted(p for p in IMPLEMENTATIONS.rglob("*") if p.suffix in {".cpp", ".h"})
    sources.extend(sorted(INTERFACES.glob("*.h")))
    command = [*compiler, "-MM", "-x", "c++", *sources]
    rules = subprocess.check_output(command, text=True).replace("\\\n", " ")
    errors = []
    for rule in rules.splitlines():
        source, *dependencies = shlex.split(rule.split(": ", 1)[1])
        owner = solver_owner(Path(source))
        for name in dependencies:
            path = Path(os.path.relpath(name))
            if path.parts[0] == "bench" or solver_owner(path) not in {None, owner, "shared"}:
                errors.append(f"{source} depends on {path}. Keep each solver independent. "
                              "Move common implementation to src/solvers/shared/.")
    if errors:
        sys.exit("\n".join(errors))


if __name__ == "__main__":
    main(sys.argv[1:] or ["c++", "-std=c++17", "-Iinclude"])
