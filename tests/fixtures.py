"""Generate tiny ISO9660 discs that exercise native reader trust boundaries."""
import io
from pathlib import Path
import sys
import pycdlib

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
for name, serial in [('wrong_hash', 'SLUS_201.84'), ('wrong_region', 'SLPM_650.22'), ('wrong_boot', 'SLUS_201.84')]:
    disc = pycdlib.PyCdlib()
    disc.new(interchange_level=3)
    boot = f'BOOT2 = cdrom0:\\{serial};1\nVER = 1.00\nVMODE = NTSC\n'.encode()
    if name == 'wrong_boot':
        boot = b'# SLUS_201.84 is mentioned here\nBOOT2 = cdrom0:\\SLPM_650.22;1\n'
    for filename, data in [(serial, b'not-a-retail-executable'), ('SYSTEM.CNF', boot), ('TEST.AFS', b'AFS\0')]:
        disc.add_fp(io.BytesIO(data), len(data), iso_path=f'/{filename};1')
    disc.write(str(root / f'{name}.iso'))
    disc.close()

original = bytearray((root / 'wrong_hash.iso').read_bytes())
(root / 'truncated.iso').write_bytes(original[:33000])
extent = int.from_bytes(original[16 * 2048 + 158:16 * 2048 + 162], 'little')
directory = extent * 2048
offset = directory
records = []
while original[offset]:
    records.append(offset)
    offset += original[offset]
file_record = records[2]
unsafe = original.copy()
unsafe[file_record + 33] = ord('/')
(root / 'unsafe.iso').write_bytes(unsafe)
bad_extent = original.copy()
bad_extent[file_record + 2:file_record + 6] = (0x7fffffff).to_bytes(4, 'little')
bad_extent[file_record + 6:file_record + 10] = (0x7fffffff).to_bytes(4, 'big')
(root / 'extent.iso').write_bytes(bad_extent)
cycle = original.copy()
cycle[file_record + 25] |= 2
cycle[file_record + 2:file_record + 10] = cycle[directory + 2:directory + 10]
cycle[file_record + 10:file_record + 18] = cycle[directory + 10:directory + 18]
(root / 'cycle.iso').write_bytes(cycle)
print(f'Generated seven disc fixtures in {root}')
