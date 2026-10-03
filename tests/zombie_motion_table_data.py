"""Verify native immutable tables retain every pinned public source entry."""
from pathlib import Path
import re
root = Path(__file__).resolve().parent.parent
original = (root/'upstream/recvx-decomp/src/ps2/veronica/prog/en01.c').read_text(encoding='utf-8')
native = (root/'src/zombie_motion_tables.hpp').read_text(encoding='utf-8')
def numbers(source, name):
    begin = source.index('{', source.index(name))
    level = 0
    for end in range(begin, len(source)):
        if source[end] == '{': level += 1
        elif source[end] == '}':
            level -= 1
            if not level:
                return list(map(int, re.findall(r'-?\d+', source[begin:end+1])))
    raise AssertionError(f'Unterminated table {name}')
for old, new, size in [('en01_mtn_tbl', 'zombie_motion_fixes', 260),
                       ('en01_mtn_tbl2', 'zombie_motion_sounds', 540),
                       ('en01_mtn_tbl3', 'zombie_motion_vibrations', 144),
                       ('en01_tree', 'zombie_fix_paths', 64)]:
    expected = numbers(original, old)
    assert len(expected) == size, (old, len(expected), size)
    if old == 'en01_tree': expected = [-1 if value == 255 else value for value in expected]
    actual = numbers(native, new)
    assert actual == expected, f'{new} differs from pinned source entries/order/sentinels'
print('All 1008 native motion-table/path integers match pinned public source')

shape_native = (root/'src/zombie_set_motion_tables.hpp').read_text(encoding='utf-8')
def literals(source, name):
    begin = source.index('{', source.index(name))
    level = 0
    for end in range(begin, len(source)):
        if source[end] == '{': level += 1
        elif source[end] == '}':
            level -= 1
            if not level:
                tokens = re.findall(r'-?(?:0x[0-9A-Fa-f]+|\d+(?:\.\d+)?)', source[begin:end+1])
                return [int(value, 16) if '0x' in value else float(value) for value in tokens]
    raise AssertionError(f'Unterminated source table {name}')
for old, new, size in [('en01_kamikami', 'zombie_bite_shape', 71),
                       ('en01_kamikami2', 'zombie_bite_shape2', 70),
                       ('en01_mogmog', 'zombie_chew_shape', 40),
                       ('rot_tbl', 'zombie_damage_rotation', 32)]:
    expected = literals(original, old)
    assert len(expected) == size, (old, len(expected), size)
    assert literals(shape_native, new) == expected, f'{new} differs from pinned source'
print('All 181 mouth scalars and 32 recoil integers match pinned public source')
