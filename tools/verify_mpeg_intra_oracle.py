#!/usr/bin/env python3
"""Compare four owned 16x16 intra samples with FFmpeg, not game/BIOS data.

The accepted sample difference is at most one: this is a limited integer-IDCT
comparison, not a claim of PS2 bit equivalence or a complete MPEG decoder.
Build tests/test_ipu_mpeg_reference_r1337.c with build_host_core.py first.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--driver',required=True,type=Path)
p.add_argument('--ffmpeg',default='ffmpeg')
a=p.parse_args()
driver=a.driver.resolve()
patterns={
 'gray':bytes([128])*384,
 'ramp':bytes(32+x*8+y*3 for y in range(16) for x in range(16))+bytes([128])*128,
 'checker':bytes(48 if (x//4+y//4)%2 else 192 for y in range(16) for x in range(16))+bytes([96])*64+bytes([160])*64,
 'chroma':bytes([128])*256+bytes(72+x*12+y*4 for y in range(8) for x in range(8))+bytes(180-x*8-y*4 for y in range(8) for x in range(8)),
}
print(subprocess.check_output([a.ffmpeg,'-version'],text=True).splitlines()[0],flush=True)
with tempfile.TemporaryDirectory(prefix='gekko2-mpeg-oracle-') as directory:
 d=Path(directory)
 for name,data in patterns.items():
  raw=d/(name+'.yuv');movie=d/(name+'.m2v');ref=d/(name+'-ref.yuv');out=d/(name+'-ipu.yuv')
  raw.write_bytes(data)
  subprocess.run([a.ffmpeg,'-hide_banner','-loglevel','error','-f','rawvideo','-pixel_format','yuv420p','-video_size','16x16','-i',str(raw),'-frames:v','1','-c:v','mpeg2video','-g','1','-bf','0','-q:v','2','-f','mpeg2video',str(movie),'-y'],check=True)
  subprocess.run([a.ffmpeg,'-hide_banner','-loglevel','error','-i',str(movie),'-frames:v','1','-pix_fmt','yuv420p','-f','rawvideo',str(ref),'-y'],check=True)
  subprocess.run([str(driver),str(movie),str(out)],check=True)
  expected,actual=ref.read_bytes(),out.read_bytes()
  assert len(expected)==len(actual)==384,(name,len(expected),len(actual))
  errors=[abs(x-y) for x,y in zip(expected,actual)]
  assert max(errors)<=1,(name,'sample difference',max(errors))
  if name=='gray':assert actual==expected,'constant gray must be exact'
  print(f'PASS {name}: samples=384 different={sum(e>0 for e in errors)} max_difference={max(errors)}',flush=True)
print('PASS 4 synthetic single-intra MPEG2 patterns / 1536 samples; not PS2 bit-exact or inter-frame coverage')
