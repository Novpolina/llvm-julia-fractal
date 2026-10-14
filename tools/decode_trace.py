"""Print the losslessly compressed runtime trace in human-readable form."""
import argparse
import gzip
import json
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--kind", choices=["uses", "instructions"], default="uses")
    parser.add_argument("--limit", type=int, help="omit to decode the entire trace")
    args = parser.parse_args()
    names = json.loads((args.directory / "opcodes.json").read_text())
    width = 2 if args.kind == "uses" else 1
    count = 0
    with gzip.open(args.directory / f"{args.kind}.bin.gz", "rb") as trace:
        while data := trace.read(65536):
            if len(data) % width:
                raise ValueError("truncated edge record")
            for start in range(0, len(data), width):
                if args.limit is not None and count >= args.limit:
                    return
                line = names[str(data[start])]
                if width == 2:
                    line += " <- " + names[str(data[start + 1])]
                print(line)
                count += 1


if __name__ == "__main__":
    try:
        main()
    except BrokenPipeError:
        sys.exit(0)
