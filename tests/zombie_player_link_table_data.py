"""Independently compare all PlayerLink table float32 words with pinned MIT source."""
from pathlib import Path
import re
import struct

root = Path(__file__).resolve().parent.parent
original = (root/'upstream/recvx-decomp/src/ps2/veronica/prog/en01.c').read_text(encoding='utf-8')
native = (root/'src/zombie_player_link_tables.hpp').read_text(encoding='utf-8')

def words(text, name):
    begin = text.index('{', text.index(name))
    level = 0
    for end in range(begin, len(text)):
        if text[end] == '{': level += 1
        elif text[end] == '}':
            level -= 1
            if not level:
                literals = re.findall(r'[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?f', text[begin:end+1])
                return [struct.pack('<f', float(value[:-1])) for value in literals]
    raise AssertionError(f'Unterminated table {name}')

count = 0
for prefix in ['cl', 'ch', 'st']:
    for clip, frames in [(1, 60), (2, 60), (4, 50), (5, 50)]:
        name = f'{prefix}_mtn{clip:02}_xz'
        expected, actual = words(original, name), words(native, name)
        assert len(expected) == frames*2, (name, len(expected))
        assert actual == expected, f'{name} differs from source float32 words/order/count'
        count += len(actual)
assert count == 1320
print('All 1320 native PlayerLink float32 words match pinned public source')
