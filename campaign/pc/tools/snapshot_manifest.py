"""Patch a fixed-size room snapshot manifest into a linked i386 PE executable.

The linker map selects original game input sections and a small glue allowlist.
Only the already-reserved .ahsnap raw section is changed: no link addresses move.
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re
import struct
import subprocess
import zlib

MAGIC = b'AHSMAP02'
VERSION = 2
BASE = 0x30000000
LIMIT = 0x40000000
MAX_REGIONS = 4096
MANIFEST_BYTES = 24 + MAX_REGIONS * 8
REQUIRED_SYMBOLS = ('sysp', 'romp', 'cam', 'ply', 'ene', 'freemem', 'Ps2_malloc_mem', 'bhEtask', 'swork')
SAFE_GLUE = frozenset(('actor_glue', 'appearance_glue', 'enemyopt_glue', 'knife_glue', 'modern_glue', 'native_glue', 'script_rules', 'spawn_glue', 'stairs_glue', 'texture_glue', 'tps_glue', 'variant_glue', 'view_glue', 'ui_glue'))

@dataclass(frozen=True)
class Region:
    address: int
    size: int
    origins: tuple[str, ...] = ()


def allowed_origin(origin: str) -> bool:
    path = origin.replace('\\', '/').strip().strip('"\'').lower()
    if re.search(r'(?:^|/)upstream/recvx-campaign-decomp/src/ps2/veronica/prog/[^/()]+\.c\.(?:obj|o)\)?$', path):
        return True
    glue = re.search(r'(?:^|/)(?:cmakefiles/recvx_game\.dir/glue|campaign/pc/glue)/([^/()]+)\.c\.(?:obj|o)\)?$', path)
    return bool(glue and glue.group(1) in SAFE_GLUE)


def merge_regions(regions: list[Region]) -> list[Region]:
    merged: list[Region] = []
    for region in sorted(regions, key=lambda r: r.address):
        if region.size <= 0:
            raise ValueError('manifest regions must have positive size')
        if merged and region.address < merged[-1].address + merged[-1].size:
            raise ValueError(f'overlapping input sections at 0x{region.address:08x}')
        if merged and region.address == merged[-1].address + merged[-1].size:
            previous = merged.pop()
            merged.append(Region(previous.address, previous.size + region.size, tuple(dict.fromkeys(previous.origins + region.origins))))
        else:
            merged.append(region)
    return merged


def parse_map(text: str) -> list[Region]:
    # GNU ld lists discarded input sections before the live memory map. Their
    # addresses are often zero and must never become snapshot destinations.
    marker = 'Linker script and memory map'
    if marker in text:
        text = text.split(marker, 1)[1]
    regions: list[Region] = []
    pending = False
    section_line = re.compile(r'^\s+(\.(?:data|bss|rdata)(?:[.$][^\s]*)?)\s*(.*)$')
    values = re.compile(r'^\s*(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(.+?)\s*$')
    for line in text.splitlines():
        match = section_line.match(line)
        if match:
            pending = not match.group(2).strip()
            body = match.group(2)
        elif pending:
            body = line
            pending = False
        else:
            continue
        match = values.match(body)
        if not match:
            continue
        address, size = int(match.group(1), 16), int(match.group(2), 16)
        origin = match.group(3)
        if size and allowed_origin(origin):
            regions.append(Region(address, size, (origin,)))
    return merge_regions(regions)


def parse_nm(text: str) -> dict[str, int]:
    symbols: dict[str, int] = {}
    for line in text.splitlines():
        match = re.match(r'^\s*([0-9a-fA-F]+)\s+[A-Za-z]\s+(\S+)\s*$', line)
        if match:
            name = match.group(2)
            if name.startswith('_'):
                name = name[1:]
            address = int(match.group(1), 16)
            if name in REQUIRED_SYMBOLS and name in symbols and symbols[name] != address:
                raise ValueError(f'ambiguous symbol {name}')
            symbols[name] = address
    return symbols


@dataclass(frozen=True)
class Section:
    name: str
    address: int
    virtual_size: int
    raw_size: int
    raw_offset: int
    flags: int


def pe_sections(data: bytes) -> tuple[int, list[Section]]:
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError('not a PE executable')
    pe = struct.unpack_from('<I', data, 60)[0]
    if pe > len(data) - 24 or data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('invalid PE signature')
    machine, count = struct.unpack_from('<HH', data, pe + 4)
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    optional = pe + 24
    if machine != 0x14c or optional_size < 96 or optional + optional_size > len(data):
        raise ValueError('room snapshots require i386 PE32')
    if struct.unpack_from('<H', data, optional)[0] != 0x10b:
        raise ValueError('room snapshots require PE32')
    base = struct.unpack_from('<I', data, optional + 28)[0]
    size = struct.unpack_from('<I', data, optional + 56)[0]
    dll_flags = struct.unpack_from('<H', data, optional + 70)[0]
    if base != BASE or not size or base + size > 0x100000000 or dll_flags & 0x40:
        raise ValueError('PE must use fixed base 0x30000000 without DYNAMIC_BASE')
    table = optional + optional_size
    if not count or table + count * 40 > len(data):
        raise ValueError('invalid PE section table')
    sections = []
    for i in range(count):
        at = table + i * 40
        name = data[at:at + 8].split(b'\0', 1)[0].decode('ascii', errors='strict')
        virtual_size, rva, raw_size, raw_offset = struct.unpack_from('<IIII', data, at + 8)
        flags = struct.unpack_from('<I', data, at + 36)[0]
        if rva + virtual_size > size or (raw_size and (raw_offset < table + count * 40 or raw_offset + raw_size > len(data))):
            raise ValueError(f'invalid bounds for PE section {name}')
        # Debug information can extend SizeOfImage past the EE pointer window.
        # Match memory.c: only persistent sections must remain below 0x40000000.
        if not flags & 0x02000000 and base + rva + virtual_size > LIMIT:
            raise ValueError(f'non-discardable PE section {name} exceeds the fixed address window')
        sections.append(Section(name, base + rva, virtual_size, raw_size, raw_offset, flags))
    return size, sections


def patch_image(data: bytes, regions: list[Region], symbols: dict[str, int]) -> bytes:
    image_size, sections = pe_sections(data)
    reserved = [s for s in sections if s.name == '.ahsnap']
    if len(reserved) != 1 or reserved[0].raw_size < MANIFEST_BYTES or reserved[0].virtual_size < MANIFEST_BYTES or reserved[0].flags & 0x22000000:
        raise ValueError('missing or undersized .ahsnap reserved section')
    slot = reserved[0]
    if data[slot.raw_offset:slot.raw_offset + 12] != MAGIC + struct.pack('<I', VERSION):
        raise ValueError('.ahsnap does not contain the reserved manifest')
    regions = merge_regions(regions)
    if not regions or len(regions) > MAX_REGIONS:
        raise ValueError('manifest has no selected regions or exceeds capacity')
    total = 0
    for region in regions:
        end = region.address + region.size
        if region.address < BASE or end > BASE + image_size or end > LIMIT:
            raise ValueError('selected region is outside the fixed image')
        # Do not allow a range to pass through unmapped padding or host code.
        cursor = region.address
        for section in sorted(sections, key=lambda s: s.address):
            if section.name not in ('.data', '.bss', '.rdata') or section.flags & 0x22000000:
                continue
            if section.address <= cursor < section.address + section.virtual_size:
                cursor = min(end, section.address + section.virtual_size)
                if cursor == end:
                    break
        if cursor != end:
            raise ValueError('selected region is not entirely in PE data sections')
        total += region.size
    if total > 256 * 1024 * 1024:
        raise ValueError('manifest exceeds 256 MiB')
    for name in REQUIRED_SYMBOLS:
        address = symbols.get(name)
        if address is None or not any(r.address <= address < r.address + r.size for r in regions):
            raise ValueError(f'critical game symbol {name} is missing from selected ranges')
    descriptors = b''.join(struct.pack('<II', r.address, r.size) for r in regions)
    header = struct.pack('<8sIIII', MAGIC, VERSION, len(regions), total, zlib.crc32(descriptors))
    result = bytearray(data)
    # Also clear raw section alignment padding, so repeat patches are deterministic.
    result[slot.raw_offset:slot.raw_offset + slot.raw_size] = (header + descriptors).ljust(slot.raw_size, b'\0')
    return bytes(result)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--map', type=Path, required=True)
    parser.add_argument('--nm', required=True)
    parser.add_argument('--audit', type=Path)
    args = parser.parse_args(argv)
    try:
        regions = parse_map(args.map.read_text(encoding='utf-8', errors='replace'))
        nm = subprocess.run([args.nm, '--defined-only', '--numeric-sort', str(args.exe)], check=True, capture_output=True, text=True, encoding='utf-8', errors='replace')
        symbols = parse_nm(nm.stdout)
        original = args.exe.read_bytes()
        patched = patch_image(original, regions, symbols)
        _, sections = pe_sections(original)
        slot = next(s for s in sections if s.name == '.ahsnap')
        with args.exe.open('r+b') as executable:
            executable.seek(slot.raw_offset)
            payload = patched[slot.raw_offset:slot.raw_offset + slot.raw_size]
            if executable.write(payload) != len(payload):
                raise OSError('short write while patching .ahsnap')
            executable.flush()
        audit = {'version': VERSION, 'base': BASE, 'count': len(regions), 'total_bytes': sum(r.size for r in regions), 'descriptor_crc32': zlib.crc32(b''.join(struct.pack('<II', r.address, r.size) for r in regions)), 'critical_symbols': {n: symbols[n] for n in REQUIRED_SYMBOLS}, 'regions': [{'address': r.address, 'size': r.size, 'origins': list(r.origins)} for r in regions]}
        if args.audit:
            args.audit.write_text(json.dumps(audit, indent=2) + '\n', encoding='utf-8')
        print(f'snapshot manifest: {audit["count"]} ranges, {audit["total_bytes"]} bytes, CRC32 {audit["descriptor_crc32"]:08x}')
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError, struct.error) as error:
        parser.exit(1, f'snapshot manifest: {error}\n')

if __name__ == '__main__':
    raise SystemExit(main())
