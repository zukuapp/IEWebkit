"""Keep source downloads, builds and binary artifacts outside the source repository."""
from pathlib import Path
import sys

REPOSITORY = Path(__file__).resolve().parent.parent

def external_work_dir(value):
    path = Path(value).expanduser().resolve()
    if path == REPOSITORY or REPOSITORY in path.parents:
        raise SystemExit(f"Build/source destination must be outside the repository: {path}")
    return path

if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit("Usage: paths.py EXTERNAL_PATH [EXTERNAL_PATH ...]")
    for argument in sys.argv[1:]:
        external_work_dir(argument)
