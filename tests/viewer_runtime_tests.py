"""Headless texture-upload/menu test with an entirely synthetic compressed room."""
import os, struct, subprocess, sys, configparser, csv
from pathlib import Path
exe,root=Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve()
root.mkdir(parents=True,exist_ok=True)
b=bytearray(4100)
def u(p,v):struct.pack_into('<I',b,p,v)
def h(p,v):struct.pack_into('<H',b,p,v)
def f(p,v):struct.pack_into('<f',b,p,v)
f(0,10);u(16,128);u(128,1132);u(256,2);u(20,2600);u(2600,2700);u(32,3240);u(3240,1);u(3244,3300)
for cut in range(2):
 for v in range(2):u(1132+cut*680+32+v*196+84,16384)
m=2700;base=m+16;b[m:m+4]=b'MDL\x80';h(m+4,16);h(m+6,1);u(m+12,0)
u(base,7);u(base+4,64);u(base+44,0xffffffff);u(base+48,0xffffffff)
u(base+64,96);u(base+68,272);p=base+96;b[p]=51;h(p+2,39);h(p+6,3)
for i,pos in enumerate([(-1,-1,-3),(1,-1,-3),(0,1,-3)]):
 for j,value in enumerate(pos):f(p+64+i*32+j*4,value)
b[p+160]=255;p=base+272;b[p]=66;h(p+2,11);h(p+4,1);h(p+6,3)
for i in range(3):h(p+8+i*6,i)
b[p+26]=255
b[3300:3304]=b'TIM2';u(3304,68);t=3332;b[t:t+4]=b'TIM2';b[t+4]=4;h(t+6,1)
u(t+16,52);u(t+24,4);h(t+28,48);b[t+35]=3;h(t+36,1);h(t+38,1);b[t+64:t+68]=bytes([255,0,0,128]);u(3400,0xffffffff)
# Literal-only Expand stream with descriptor/data interleaving and terminal bits.
encoded=bytearray();cursor=0
while len(b)-cursor>=8:encoded.append(255);encoded.extend(b[cursor:cursor+8]);cursor+=8
tail=b[cursor:];encoded.append(((1<<len(tail))-1)|(1<<(len(tail)+1)));encoded.extend(tail);encoded.extend(b'\0\0')
archive=root/'日本語-synthetic.AFS';archive.write_bytes(b'AFS\0'+struct.pack('<IIIII',2,24,len(encoded),24+len(encoded),len(encoded))+encoded+encoded)
env=os.environ.copy();env['SDL_VIDEODRIVER']='dummy';env['SDL_RENDER_DRIVER']='software'
ini=root/'profile/AshfordsHalberd.ini'
ini.parent.mkdir(exist_ok=True)
ini.write_text('[Viewer]\nroom=9999\ncut=4095\nvariant=1\n')
for launch in range(2):
    before=configparser.ConfigParser();before.read(ini);variant=before.getint('Viewer','variant',fallback=0)
    result=subprocess.run([str(exe),'--scene-smoke-test','--archive',str(archive),'--data-dir',str(root/'profile')],env=env,capture_output=True,timeout=60)
    assert result.returncode==0,(result.returncode,result.stderr.decode(errors='replace'))
    report=(root/'profile/scene-smoke.txt').read_text()
    assert int(report.split('red_pixels=')[1].strip())>1000,report
    after=configparser.ConfigParser();after.read(ini)
    assert after.getint('Viewer','variant')==1-variant
    assert after.getint('Viewer','room')==0 and after.getint('Viewer','cut')==0
    trace=list(csv.DictReader((root/'profile/scene-navigation.tsv').open(),delimiter='\t'))
    assert [(int(row['room']),int(row['cut'])) for row in trace]==[(0,0),(0,0),(0,1),(0,0),(1,0),(0,0),(1,0),(0,0)],trace
    assert (root/'profile/scene-menu-return.txt').exists()
print('Headless viewer upload, camera selection and settings persistence passed')
