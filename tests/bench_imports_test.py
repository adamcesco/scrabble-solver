"""Check that each benchmark measurement owns its implementation."""
import ast
import unittest
from importlib.util import resolve_name
from pathlib import Path


def imports(path):
    package = ".".join(path.parent.parts)
    for node in ast.walk(ast.parse(path.read_text(), filename=str(path))):
        if isinstance(node, ast.Import):
            for alias in node.names:
                yield node.lineno, alias.name
        elif isinstance(node, ast.ImportFrom):
            parent = resolve_name("." * node.level + (node.module or ""), package)
            for alias in node.names:
                yield node.lineno, f"{parent}.{alias.name}"


class BenchmarkImportsTest(unittest.TestCase):
    def test_measurements_and_support_do_not_reach_into_other_measurements(self):
        sources = sorted(Path("bench").rglob("*.py"))
        packages = {path.parts[1] for path in sources if len(path.parts) > 2}
        measurements = packages - {"corpus", "measurement_support", "fixtures", "results"}
        violations = []
        for path in sources:
            source = path.parts[1]
            if source not in measurements and source != "measurement_support":
                continue
            for line, name in imports(path):
                package, _, rest = name.partition(".")
                owner = rest.split(".", 1)[0]
                if package == "bench" and owner in measurements and owner != source:
                    violations.append(f"{path}:{line}: {name} belongs to bench/{owner}/. "
                                      "Put code used by multiple measurements in bench/measurement_support/.")
        if violations:
            self.fail("\n" + "\n".join(violations))


if __name__ == "__main__":
    unittest.main()
