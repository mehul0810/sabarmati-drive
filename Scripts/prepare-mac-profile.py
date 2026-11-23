"""Seed the shared Mac game's offline profile once from the existing checkout."""
import argparse
import os
import shutil
import tempfile
from pathlib import Path


def seed(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists() or not source.is_file():
        return
    with tempfile.NamedTemporaryFile(dir=target.parent, prefix=".profile-", delete=False) as output:
        temporary = Path(output.name)
        try:
            with source.open("rb") as saved:
                shutil.copyfileobj(saved, output)
            output.flush()
            os.fsync(output.fileno())
            try:
                # Link an already complete file. A concurrent launch can win safely.
                os.link(temporary, target)
            except FileExistsError:
                pass
        finally:
            temporary.unlink(missing_ok=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("target", type=Path)
    args = parser.parse_args()
    seed(args.source, args.target)
