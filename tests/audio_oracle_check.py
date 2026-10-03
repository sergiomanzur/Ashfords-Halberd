"""Compare private native WAV exports to the pinned headless vgmstream CLI.

The independent decoder includes ADPCM terminal/padding frames. The native
decoder stops at the encoded end; compare the full native prefix, never trim
native samples to obtain a match. No audio device is opened by either tool.
"""
import array
import csv
import json
import pathlib
import subprocess
import sys
import wave


def pcm(path):
    with wave.open(str(path), "rb") as source:
        assert source.getnchannels() == 1 and source.getsampwidth() == 2, path
        result = array.array("h", source.readframes(source.getnframes()))
        if sys.byteorder != "little":
            result.byteswap()
        return source.getframerate(), result


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    reports = []
    for argument in sys.argv[2:]:
        root = pathlib.Path(argument).resolve()
        oracle = root / "oracle"
        oracle.mkdir()  # A fresh directory preserves each verification run.
        with (root / "samples.tsv").open(newline="") as stream:
            rows = list(csv.DictReader(stream, delimiter="\t"))
        total_frames, largest, nonzero = 0, 0, 0
        for row in rows:
            reference = oracle / row["wav"]
            result = subprocess.run([str(executable), "-i", "-s", str(int(row["wave"]) + 1),
                                     "-o", str(reference), str(root / row["header"])],
                                    capture_output=True, text=True, timeout=30,
                                    creationflags=subprocess.CREATE_NO_WINDOW)
            assert result.returncode == 0, result.stdout + result.stderr
            rate, native = pcm(root / row["wav"])
            oracle_rate, expected = pcm(reference)
            assert rate == oracle_rate == int(row["rate"]), row
            assert len(native) == int(row["frames"]) and len(expected) >= len(native), row
            difference = max((abs(a - b) for a, b in zip(native, expected)), default=0)
            # Vgmstream evaluates float predictor coefficients; the native
            # decoder uses exact rational floor arithmetic. One PCM16 unit is
            # the maximum allowed quantization difference, never a waveform fit.
            assert difference <= 1, (row["wav"], difference)
            largest = max(largest, difference)
            total_frames += len(native)
            nonzero += int(any(native))
        assert rows and nonzero, "No actual sound samples decoded"
        report = {"directory": root.name, "waves": len(rows), "frames": total_frames,
                  "nonzero_waves": nonzero, "maximum_pcm16_difference": largest}
        (oracle / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
        reports.append(report)
        print(json.dumps(report), flush=True)
    print("Independent original sample decoding verified; no audio playback performed.")


if __name__ == "__main__":
    main()
