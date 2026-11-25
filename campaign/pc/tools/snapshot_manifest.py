"""Patch a fixed-size room snapshot manifest into a linked i386 PE executable
or an Android arm64 ELF64 shared library (librecvx_game.so).

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
SAFE_GLUE = frozenset(('actor_glue', 'appearance_glue', 'biorand_glue', 'enemyopt_glue', 'extras_glue', 'knife_glue', 'modern_glue', 'native_glue', 'script_rules', 'spawn_glue', 'stairs_glue', 'texture_glue', 'tps_glue', 'variant_glue', 'view_glue', 'ui_glue'))

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


def parse_lld_map(text: str, base: int = BASE) -> list[Region]:
    """Parse an LLD -Map file for an Android arm64 windowed shared library.

    LLD emits 5-column rows: VMA LMA Size Align <name>, where output sections
    have a single space after Align and input sections are indented further as
    `<object_path>:(<input_section>)`. Segment-relative addresses in the data
    window (< LIMIT - BASE) are shifted by `base` (0x30000000).
    """
    allowed_outputs = {'.rodata', '.data', '.bss', '.data.rel.ro'}
    row_re = re.compile(r'^\s*([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+(\d+)( +)(\S.*)$')
    input_re = re.compile(r'^(.+?):\(([^)]+)\)$')
    cur_output: str | None = None
    regions: list[Region] = []
    for line in text.splitlines():
        m = row_re.match(line)
        if not m:
            continue
        vma = int(m.group(1), 16)
        size = int(m.group(3), 16)
        spaces = m.group(5)
        rest = m.group(6).strip()
        if len(spaces) == 1:
            cur_output = rest.split()[0]
            continue
        if cur_output not in allowed_outputs or not size:
            continue
        m_in = input_re.match(rest)
        if not m_in:
            continue
        origin = m_in.group(1).strip()
        if not allowed_origin(origin):
            continue
        addr = base + vma if vma < LIMIT - BASE else vma
        regions.append(Region(addr, size, (origin,)))
    return merge_regions(regions)


def parse_nm(text: str, base: int = 0) -> dict[str, int]:
    symbols: dict[str, int] = {}
    for line in text.splitlines():
        match = re.match(r'^\s*([0-9a-fA-F]+)\s+[A-Za-z]\s+(\S+)\s*$', line)
        if match:
            name = match.group(2)
            if name.startswith('_'):
                name = name[1:]
            address = int(match.group(1), 16)
            if base and address < LIMIT - BASE:
                address += base
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


def elf64_sections(data: bytes, base: int = BASE) -> tuple[int, list[Section]]:
    """Parse an AArch64 ELF64 shared object linked with android-window.ld."""
    if len(data) < 64 or data[:4] != b'\x7fELF':
        raise ValueError('not an ELF binary')
    ei_class, ei_data = data[4], data[5]
    if ei_class != 2 or ei_data != 1:
        raise ValueError('Android snapshots require little-endian ELF64')
    e_type, e_machine = struct.unpack_from('<HH', data, 16)
    if e_type != 3 or e_machine != 183:
        raise ValueError('Android snapshots require AArch64 ET_DYN shared library')
    e_phoff, e_shoff = struct.unpack_from('<QQ', data, 32)
    e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHHHH', data, 54)
    if not e_phnum or e_phentsize < 56 or e_phoff + e_phnum * e_phentsize > len(data):
        raise ValueError('invalid ELF64 program header table')
    if not e_shnum or e_shentsize < 64 or e_shoff + e_shnum * e_shentsize > len(data):
        raise ValueError('invalid ELF64 section header table')
    if e_shstrndx >= e_shnum:
        raise ValueError('invalid ELF64 shstrtab index')

    window_span = LIMIT - BASE
    min_vaddr: int | None = None
    data_max_vaddr = 0
    has_exec = False
    for i in range(e_phnum):
        at = e_phoff + i * e_phentsize
        p_type, p_flags, p_offset, p_vaddr, _p_paddr, p_filesz, p_memsz, _p_align = struct.unpack_from('<IIQQQQQQ', data, at)
        if p_type != 1:  # PT_LOAD
            continue
        if p_filesz and p_offset + p_filesz > len(data):
            raise ValueError(f'invalid file bounds for PT_LOAD[{i}]')
        if (p_flags & 0x2) and (p_flags & 0x1):  # PF_W | PF_X
            raise ValueError(f'W^X violation in PT_LOAD[{i}]')
        if min_vaddr is None or p_vaddr < min_vaddr:
            min_vaddr = p_vaddr
        if p_flags & 0x1:  # PF_X
            has_exec = True
            if p_vaddr < window_span or p_vaddr + p_memsz > 0x20000000:
                raise ValueError(f'executable PT_LOAD[{i}] outside code range [0x10000000, 0x20000000)')
        else:
            if p_vaddr + p_memsz > window_span:
                raise ValueError(f'data PT_LOAD[{i}] exceeds the 256 MiB PS2 window')
            if p_vaddr + p_memsz > data_max_vaddr:
                data_max_vaddr = p_vaddr + p_memsz

    if min_vaddr != 0:
        raise ValueError(f'ELF64 min PT_LOAD vaddr must be 0 for load_bias 0x30000000 (got 0x{min_vaddr or 0:x})')
    if not data_max_vaddr or not has_exec:
        raise ValueError('ELF64 missing required data or executable PT_LOAD segments')

    str_at = e_shoff + e_shstrndx * e_shentsize
    _, _, _, _, str_off, str_size = struct.unpack_from('<IIQQQQ', data, str_at)
    if str_off + str_size > len(data):
        raise ValueError('invalid ELF64 .shstrtab bounds')
    shstrtab = data[str_off:str_off + str_size]

    sections: list[Section] = []
    for i in range(e_shnum):
        at = e_shoff + i * e_shentsize
        sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size = struct.unpack_from('<IIQQQQ', data, at)
        if not (sh_flags & 0x2):  # SHF_ALLOC
            continue
        if sh_name >= len(shstrtab):
            raise ValueError(f'invalid section name offset for section {i}')
        end_nul = shstrtab.find(b'\0', sh_name)
        name = shstrtab[sh_name:(end_nul if end_nul >= 0 else len(shstrtab))].decode('ascii', errors='strict')
        is_nobits = (sh_type == 8)
        raw_size = 0 if is_nobits else sh_size
        if raw_size and sh_offset + raw_size > len(data):
            raise ValueError(f'invalid file bounds for ELF section {name}')
        if not (sh_flags & 0x4):  # non-exec SHF_ALLOC
            # Unwind tables (.eh_frame_hdr, .eh_frame, .gcc_except_table) are placed in
            # the exec PT_LOAD [0x10000000, 0x20000000) by android-window.ld.
            if not (sh_flags & 0x1) and name in ('.eh_frame_hdr', '.eh_frame', '.gcc_except_table') and window_span <= sh_addr and sh_addr + sh_size <= 0x20000000:
                continue
            if sh_addr + sh_size > window_span:
                raise ValueError(f'non-exec ELF section {name} exceeds the fixed address window')
            sections.append(Section(name, base + sh_addr, sh_size, raw_size, sh_offset, sh_flags))
    return data_max_vaddr, sections


def _write_manifest(data: bytes, slot: Section, regions: list[Region], symbols: dict[str, int],
                    sections: list[Section], valid_names: tuple[str, ...], image_end: int,
                    reject_flags_mask: int) -> bytes:
    if data[slot.raw_offset:slot.raw_offset + 12] != MAGIC + struct.pack('<I', VERSION):
        raise ValueError('.ahsnap does not contain the reserved manifest')
    regions = merge_regions(regions)
    if not regions or len(regions) > MAX_REGIONS:
        raise ValueError('manifest has no selected regions or exceeds capacity')
    total = 0
    for region in regions:
        end = region.address + region.size
        if region.address < BASE or end > image_end or end > LIMIT:
            raise ValueError('selected region is outside the fixed image')
        cursor = region.address
        for section in sorted(sections, key=lambda s: s.address):
            if section.name not in valid_names or (section.flags & reject_flags_mask):
                continue
            if section.address <= cursor < section.address + section.virtual_size:
                cursor = min(end, section.address + section.virtual_size)
                if cursor == end:
                    break
        if cursor != end:
            raise ValueError('selected region is not entirely in image data sections')
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


def patch_image(data: bytes, regions: list[Region], symbols: dict[str, int]) -> bytes:
    image_size, sections = pe_sections(data)
    reserved = [s for s in sections if s.name == '.ahsnap']
    if len(reserved) != 1 or reserved[0].raw_size < MANIFEST_BYTES or reserved[0].virtual_size < MANIFEST_BYTES or reserved[0].flags & 0x22000000:
        raise ValueError('missing or undersized .ahsnap reserved section')
    return _write_manifest(data, reserved[0], regions, symbols, sections,
                           ('.data', '.bss', '.rdata'), BASE + image_size, 0x22000000)


def patch_elf_image(data: bytes, regions: list[Region], symbols: dict[str, int]) -> bytes:
    data_max_vaddr, sections = elf64_sections(data)
    reserved = [s for s in sections if s.name == '.ahsnap']
    if len(reserved) != 1 or reserved[0].raw_size < MANIFEST_BYTES or reserved[0].virtual_size < MANIFEST_BYTES or (reserved[0].flags & 0x4):
        raise ValueError('missing or undersized .ahsnap reserved section')
    return _write_manifest(data, reserved[0], regions, symbols, sections,
                           ('.data', '.bss', '.rodata', '.data.rel.ro'), BASE + data_max_vaddr, 0x4)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument('--exe', type=Path)
    target.add_argument('--so', type=Path)
    parser.add_argument('--map', type=Path)
    parser.add_argument('--nm', required=True)
    parser.add_argument('--audit', type=Path)
    args = parser.parse_args(argv)
    try:
        if args.exe:
            if not args.map:
                raise ValueError('--map is required with --exe')
            regions = parse_map(args.map.read_text(encoding='utf-8', errors='replace'))
            nm = subprocess.run([args.nm, '--defined-only', '--numeric-sort', str(args.exe)], check=True, capture_output=True, text=True, encoding='utf-8', errors='replace')
            symbols = parse_nm(nm.stdout)
            original = args.exe.read_bytes()
            patched = patch_image(original, regions, symbols)
            _, sections = pe_sections(original)
            out_path = args.exe
        else:
            map_path = args.map if args.map else args.so.with_suffix('.map')
            if not map_path.exists():
                raise ValueError(f'linker map not found: {map_path} (pass --map)')
            regions = parse_lld_map(map_path.read_text(encoding='utf-8', errors='replace'))
            nm = subprocess.run([args.nm, '--defined-only', '--numeric-sort', str(args.so)], check=True, capture_output=True, text=True, encoding='utf-8', errors='replace')
            symbols = parse_nm(nm.stdout, base=BASE)
            original = args.so.read_bytes()
            patched = patch_elf_image(original, regions, symbols)
            _, sections = elf64_sections(original)
            out_path = args.so
        slot = next(s for s in sections if s.name == '.ahsnap')
        with out_path.open('r+b') as executable:
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
