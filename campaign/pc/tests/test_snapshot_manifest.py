"""Disc-free GNU ld map selection and PE manifest patch tests."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest
import zlib
TOOL = Path(__file__).resolve().parents[1] / 'tools' / 'snapshot_manifest.py'
spec = importlib.util.spec_from_file_location('snapshot_manifest', TOOL)
m = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = m
spec.loader.exec_module(m)
PROG = 'CMakeFiles/recvx_game.dir/__/__/upstream/recvx-campaign-decomp/src/ps2/veronica/prog/'
GLUE = 'CMakeFiles/recvx_game.dir/glue/'
def fixture():
    p=bytearray(1024+33280);p[:2]=b'MZ';struct.pack_into('<I',p,60,128);p[128:132]=b'PE\0\0'
    struct.pack_into('<HHIIIHH',p,132,0x14c,2,0,0,0,224,0x102)
    o=152;struct.pack_into('<H',p,o,0x10b);struct.pack_into('<I',p,o+28,0x30000000);struct.pack_into('<I',p,o+56,0x100000);struct.pack_into('<H',p,o+70,0)
    for off,name,vs,va,raw,ptr,ch in [(376,b'.data',256,0x1000,512,512,0xc0000040),(416,b'.ahsnap',32792,0x2000,33280,1024,0x40000040)]:
        p[off:off+8]=name.ljust(8,b'\0');struct.pack_into('<IIII',p,off+8,vs,va,raw,ptr);struct.pack_into('<I',p,off+36,ch)
    p[1024:1048]=struct.pack('<8sIIII',b'AHSMAP02',2,0,0,0)
    return p
class ManifestTests(unittest.TestCase):
    def test_input_sections_only_and_long_names(self):
        text=f''' .data 0x30001000 0x10 {PROG}main.c.obj
 .bss.long_game_name
                0x30001010 0x20 {PROG}sub1.c.obj
 .rdata 0x30001030 0x08 {GLUE}actor_glue.c.obj
 .data 0x30001038 0x08 {GLUE}cheat_glue.c.obj
 .data 0x30001040 0x08 CMakeFiles/recvx_platform.dir/src/host/options.c.obj
.data 0x30001000 0x200
 .text 0x30001048 0x10 {PROG}main.c.obj
 .data 0x30001058 0x08 evil/prog/main.c.obj
'''
        regions=m.parse_map(text)
        self.assertEqual([(r.address,r.size) for r in regions],[(0x30001000,0x38)])
    def test_overlap_and_gaps(self):
        text=f' .data 0x30001000 0x10 {PROG}main.c.obj\n .bss 0x30001020 0x10 {PROG}sub1.c.obj\n'
        self.assertEqual(len(m.parse_map(text)),2)
        with self.assertRaises(ValueError):m.parse_map(text+f' .data 0x30001008 0x10 {PROG}event.c.obj\n')
    def test_discarded_input_sections_are_excluded(self):
        text=f'Discarded input sections\n .rdata 0x00000000 0x10 {PROG}main.c.obj\nLinker script and memory map\n .data 0x30001000 0x10 {PROG}main.c.obj\n'
        self.assertEqual([(r.address,r.size) for r in m.parse_map(text)],[(0x30001000,16)])
    def test_pe_patch_same_size_only_reserved_section(self):
        original=fixture();regions=m.parse_map(f' .data 0x30001000 0x10 {PROG}main.c.obj\n')
        symbols={name:0x30001000 for name in m.REQUIRED_SYMBOLS}
        patched=m.patch_image(bytes(original),regions,symbols)
        self.assertEqual(len(patched),len(original));self.assertEqual(patched[:1024],original[:1024]);self.assertEqual(struct.unpack_from('<IIII',patched,1032),(2,1,16,zlib.crc32(struct.pack('<II',0x30001000,16))))
        self.assertEqual(patched[1056:],bytes(len(original)-1056))
    def test_wrong_arch_base_aslr_section_and_symbols_rejected(self):
        regions=m.parse_map(f' .data 0x30001000 0x10 {PROG}main.c.obj\n');symbols={name:0x30001000 for name in m.REQUIRED_SYMBOLS}
        for offset,fmt,value in [(132,'H',0x8664),(152+28,'I',0x400000),(152+70,'H',0x40),(416+16,'I',32)]:
            p=fixture();struct.pack_into('<'+fmt,p,offset,value)
            with self.assertRaises(ValueError):m.patch_image(bytes(p),regions,symbols)
        with self.assertRaises(ValueError):m.patch_image(bytes(fixture()),regions,{})
        with self.assertRaises(ValueError):m.patch_image(bytes(fixture()),regions,{**symbols,'sysp':0x30001020})
    def test_ranges_outside_image_and_unmapped_gaps_rejected(self):
        symbols={name:0x30001000 for name in m.REQUIRED_SYMBOLS}
        for address,size in [(0x2fffffff,16),(0x300ffff0,32),(0x30001500,16),(0x30002000,16),(0x30001000,0)]:
            with self.assertRaises(ValueError):m.patch_image(bytes(fixture()),[m.Region(address,size)],symbols)
    def test_nm_underscore_strip(self):
        symbols=m.parse_nm('30001000 B _sysp\n30001010 D romp\n         U _missing\n')
        self.assertEqual(symbols,{'sysp':0x30001000,'romp':0x30001010})
    def test_nm_repeated_local_names_are_not_ambiguous_game_symbols(self):
        symbols=m.parse_nm('30001000 b _state\n30001010 b _state\n30001020 B _sysp\n')
        self.assertEqual(symbols['sysp'],0x30001020)
        with self.assertRaises(ValueError):m.parse_nm('30001000 B _sysp\n30001010 B _sysp\n')
    def test_debug_sections_outside_ps2_window_are_allowed_only_if_discardable(self):
        p=fixture();struct.pack_into('<H',p,134,3);struct.pack_into('<I',p,152+56,0x10002000)
        p[456:464]=b'.debug\0\0';struct.pack_into('<IIII',p,464,0x100,0x10001000,0,0);struct.pack_into('<I',p,492,0x42000040)
        regions=[m.Region(0x30001000,16)];symbols={name:0x30001000 for name in m.REQUIRED_SYMBOLS}
        self.assertEqual(len(m.patch_image(bytes(p),regions,symbols)),len(p))
        struct.pack_into('<I',p,492,0x40000040)
        with self.assertRaises(ValueError):m.patch_image(bytes(p),regions,symbols)
if __name__=='__main__':unittest.main()
