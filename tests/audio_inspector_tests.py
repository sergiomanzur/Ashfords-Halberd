"""Headless production inspector verification with synthetic disk fixtures."""
import csv
import pathlib
import struct
import subprocess
import sys
import tempfile
import wave


def run(command):
    return subprocess.run([str(x) for x in command], capture_output=True,
                          text=True, timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)


def main():
    exe, generator, scratch = [pathlib.Path(arg).resolve() for arg in sys.argv[1:]]
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="audio-", dir=scratch) as directory:
        root = pathlib.Path(directory)
        generated = run([generator, "write-inspector-fixture", root])
        assert generated.returncode == 0, generated.stderr
        command = [exe, root / "test.AFS", root / "test.IDX", "20", root / "output"]
        result = run(command)
        assert result.returncode == 0, result.stderr
        output = root / "output"
        assert (output / "samples.tsv").is_file(), "Inspector must export decoded PCM manifest"
        with (output / "samples.tsv").open(newline="") as stream:
            rows = list(csv.DictReader(stream, delimiter="\t"))
        assert len(rows) == 2 and rows[0]["rate"] == "32000" and rows[0]["frames"] == "28"
        assert rows[1]["loop_start"] == "0" and rows[1]["loop_end"] == "28"
        with wave.open(str(output / rows[0]["wav"]), "rb") as decoded:
            assert decoded.getnchannels() == 1 and decoded.getsampwidth() == 2
            assert struct.unpack("<4h", decoded.readframes(4)) == (1, 2, 0, 0)
        with (output / "sequences.tsv").open(newline="") as stream:
            events = list(csv.DictReader(stream, delimiter="\t"))
        assert len(events) == 12 and events[0]["type"] == "tempo"
        assert events[6]["type"] == "note_off" and events[6]["tick"] == "129"
        assert events[10]["type"] == "note_off" and events[10]["tick"] == "480"
        assert events[11]["type"] == "end" and events[0]["bank"] == "3"
        before = (output / "samples.tsv").read_bytes()
        assert run(command).returncode != 0, "Inspector must reject existing output directory"
        assert (output / "samples.tsv").read_bytes() == before
        wrong = command.copy()
        wrong[-2], wrong[-1] = "21", root / "absent-key-output"
        assert run(wrong).returncode != 0 and not wrong[-1].exists()
    print("Native audio inspector PCM/events/fresh-output checks passed")


if __name__ == "__main__":
    main()
