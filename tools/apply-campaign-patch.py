"""Apply the pinned decomp adaptation without overwriting unrelated local edits."""
import pathlib
import subprocess
import sys


def apply(tree, patch):
    prefix = ["git", "-C", str(tree), "apply"]
    patch = str(pathlib.Path(patch).resolve())
    # Already applied is the normal incremental-build case.
    if subprocess.run(prefix + ["--reverse", "--check", patch], capture_output=True).returncode == 0:
        return
    check = subprocess.run(prefix + ["--check", patch], capture_output=True, text=True)
    if check.returncode:
        raise SystemExit("Campaign adaptation conflicts with this decomp checkout. "
                         "Preserve local edits and reconcile campaign/patches/offline.patch.\n" + check.stderr)
    subprocess.run(prefix + [patch], check=True)


if __name__ == "__main__":
    apply(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]))
