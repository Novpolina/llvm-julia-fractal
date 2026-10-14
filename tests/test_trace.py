"""Check real dynamic execution, PHI expansion, frame preservation and n-grams."""
import argparse
from collections import Counter
import csv
import gzip
import json
import os
from pathlib import Path
import subprocess
import tempfile


def run(command, **kwargs):
    return subprocess.run([str(item) for item in command], check=True,
                          capture_output=True, text=True, **kwargs).stdout


def read_trace(directory, kind):
    names = json.loads((directory / "opcodes.json").read_text())
    data = gzip.decompress((directory / f"{kind}.bin.gz").read_bytes())
    if kind == "instructions":
        return [names[str(byte)] for byte in data]
    assert len(data) % 2 == 0
    return [f"{names[str(data[i])]} <- {names[str(data[i + 1])]}"
            for i in range(0, len(data), 2)]


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    bindir = Path(run([os.environ.get("LLVM_CONFIG", "llvm-config"), "--bindir"]).strip())
    plugin = args.build / "libUseTracePass.so"
    opt = [bindir / "opt", f"-load-pass-plugin={plugin}",
           "-passes=trace-uses", "-verify-each", "-S"]
    with tempfile.TemporaryDirectory(dir=args.build) as temporary:
        temp = Path(temporary)
        fixture = temp / "fixture.ll"
        run(opt + ["tests/phi_fixture.ll", "-o", fixture])
        second = temp / "second.ll"
        run(opt + [fixture, "-o", second])
        # The only textual change on a second pass is opt's ModuleID comment.
        assert fixture.read_text().splitlines()[1:] == second.read_text().splitlines()[1:]
        driver = temp / "driver.c"
        driver.write_text("extern int fixture(int, _Bool);\n"
                          "int main(void) { return fixture(3, 1) != 8; }\n")
        executable = temp / "fixture"
        run([bindir / "clang", "-O0", fixture, driver,
             args.build / "runtime.o", "-lz", "-o", executable])
        run([executable], env=dict(os.environ, IR_TRACE_DIR=str(temp.resolve())))
        instructions = read_trace(temp, "instructions")
        uses = read_trace(temp, "uses")
        assert instructions == ["br", "add", "br", "br", "mul", "add", "icmp", "br",
                                "mul", "add", "icmp", "br", "shl", "ret"]
        assert "sub" not in instructions  # The right block was never executed.
        assert uses == ["mul <- add", "mul <- sub", "mul <- add", "mul <- sub",
                        "add <- add", "icmp <- add", "br <- icmp"] * 2 + [
                            "shl <- add", "shl <- sub", "ret <- shl"]
        assert not any("phi" in item for item in uses + instructions)
        run([args.build / "analyze-trace", temp])
        for kind, events in [("instructions", instructions), ("uses", uses)]:
            for length in range(1, 6):
                expected = Counter(" ; ".join(events[i:i + length])
                                   for i in range(len(events) - length + 1))
                with (temp / f"{kind}_patterns_{length}.tsv").open() as file:
                    actual = {row["pattern"]: int(row["count"])
                              for row in csv.DictReader(file, delimiter="\t")}
                assert actual == expected
        other = temp / "sim.ll"
        other.write_text(Path("tests/phi_fixture.ll").read_text().replace(
            'source_filename = "app.c"', 'source_filename = "sim.c"'))
        run(opt + [other, "-o", second])
        assert "__ir_trace" not in second.read_text()
        # Verify the clang callback also runs after optimization.
        app = temp / "app.c"
        app.write_text("int f(int x) { return x * 2; }\n")
        run([bindir / "clang", "-O2", f"-fpass-plugin={plugin.resolve()}",
             "-S", "-emit-llvm", app, "-o", second])
        assert "call void @__ir_trace" in second.read_text()
        assert "memory(none)" not in second.read_text()

        # Large traces stay compressed; compare compressed bytes.
        for mode in ("O2", "Os"):
            directory = temp / mode
            directory.mkdir()
            environment = dict(os.environ, IR_TRACE_DIR=str(directory.resolve()), TRACE_FRAMES="2")
            baseline = run([args.build / f"baseline-{mode}"], env=environment)
            traced = run([args.build / f"trace-headless-{mode}"], env=environment)
            assert baseline == traced, (mode, baseline, traced)
            committed = Path("statistics") / mode
            if (committed / "frames.txt").exists():
                assert traced == (committed / "frames.txt").read_text()
                for kind in ("uses", "instructions"):
                    assert (directory / f"{kind}.bin.gz").read_bytes() == (
                        committed / f"{kind}.bin.gz").read_bytes(), f"SDL/headless mismatch: {mode} {kind}"
            print(f"{mode}: baseline = instrumented = SDL; two complete frames")
    print("trace tests passed: dynamic branches, nested/cyclic PHI, multiplicity, "
          "module filter, idempotence, clang callback, windows 1..5, frame checksums")


if __name__ == "__main__":
    main()
