"""Validate a user-supplied USA disc and save local analysis inputs only."""
import argparse
import hashlib
import io
import json
from pathlib import Path

import pycdlib
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("iso", type=Path)
    args = parser.parse_args()
    iso = pycdlib.PyCdlib()
    iso.open(str(args.iso.resolve()))
    try:
        executable = io.BytesIO()
        config = io.BytesIO()
        iso.get_file_from_iso_fp(executable, iso_path="/SLUS_201.84;1")
        iso.get_file_from_iso_fp(config, iso_path="/SYSTEM.CNF;1")
        data = executable.getvalue()
        sha1 = hashlib.sha1(data).hexdigest()
        upstream_config = ROOT / "upstream/recvx-decomp/config/SLUS_201.84.yaml"
        expected = next(line.split(":", 1)[1].strip() for line in
                        upstream_config.read_text().splitlines() if line.startswith("sha1:"))
        if sha1 != expected:
            raise ValueError(f"Executable mismatch: expected {expected}, got {sha1}")
        elf = ELFFile(io.BytesIO(data))
        if elf.header["e_machine"] != "EM_MIPS" or elf.elfclass != 32:
            raise ValueError("Expected the 32-bit MIPS executable")
        output = ROOT / "local/disc"
        output.mkdir(parents=True, exist_ok=True)
        (output / "SLUS_201.84").write_bytes(data)
        (output / "SYSTEM.CNF").write_bytes(config.getvalue())
        files = []
        for parent, _, names in iso.walk(iso_path="/"):
            files.extend(f"{parent.rstrip('/')}/{name}" for name in names)
        report = {"region": "USA", "serial": "SLUS-20184", "executable_sha1": sha1,
                  "elf_machine": elf.header["e_machine"], "elf_bits": elf.elfclass,
                  "disc_size": args.iso.stat().st_size, "files": sorted(files)}
        (output / "inspection.json").write_text(json.dumps(report, indent=2) + "\n")
        print(f"Verified CVX USA executable: {sha1}; {len(files)} files. Local report: {output}")
    finally:
        iso.close()


if __name__ == "__main__":
    main()
