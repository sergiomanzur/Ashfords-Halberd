"""Verify all event constants against the pinned public decompilation."""
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / "upstream/recvx-decomp/src/ps2/veronica/prog/en01.c").read_text()
native = (root / "src/zombie_effect_tables.hpp").read_text()


def values(text, name):
    body = re.search(r"\b" + name + r"\[\d+\]\s*=\s*\{(.*?)\};", text, re.S)
    assert body, f"Missing table {name}"
    return [float(n.rstrip("f")) for n in re.findall(r"-?\d+(?:\.\d+)?f?", body[1])]


for name, count in (("en01_keff_tbl", 55), ("en01_weff_tbl", 288)):
    expected, actual = values(source, name), values(native, name)
    assert len(expected) == count, f"Unexpected source table size {name}"
    assert actual == expected, f"Native effect table differs: {name}"
print("All 343 Kami/Water event constants match pinned source")
