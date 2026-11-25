#!/usr/bin/env python3
"""GPL-3.0-or-later. Execute CFG-generated EE AOT with real interpreter fallback.

Only a synthetic ELF is used. This host check is not Wii execution or gameplay.
"""
import importlib.util
from pathlib import Path
import re
import runpy
import subprocess
import struct
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('hybrid',Path(__file__).with_name('hybrid_compile.py'))
h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)
sys.path.insert(0,str(ROOT/'tests'))
from test_native_hybrid_compile import elf
source=ROOT/'tests/test_native_hybrid_execute.c'
words=[int(w,16) for w in re.findall(r'0x([a-f0-9]+)',source.read_text().split('static const uint32_t code[]={',1)[1].split('};',1)[0])]
with tempfile.TemporaryDirectory(prefix='gekko-hybrid-') as tmp:
 d=Path(tmp);image=bytearray(elf(words));struct.pack_into('<I',image,24,0x80200000);struct.pack_into('<II',image,84-32+8,0x80200000,0x80200000)
 p=d/'synthetic.elf';p.write_bytes(image)
 generated=d/'generated';h.compile_elf(p,generated,[0x80200000])
 # Reuse the ordinary host core cache. Only the EE core and AOT unit need
 # rebuilding with private include overrides; production includes stay intact.
 saved=sys.argv;sys.argv=['tools/build_host_core.py']
 try:s=runpy.run_path(str(ROOT/'tools/build_host_core.py'))
 finally:sys.argv=saved
 flags=['-DGEKKO2_EE_AOT','-I'+str(generated),*s['flags']]
 objects=[]
 for path,obj in s['objects'].items():
  if path==ROOT/'source/core/ee/ee_core.c' or path==ROOT/'source/core/recompiler/ee_aot.c':
   src=generated/'ee_aot.c' if path.name=='ee_aot.c' else path
   obj=d/(path.stem+'.o')
   subprocess.run(['gcc',*flags,'-c',str(src),'-o',str(obj)],cwd=ROOT,check=True)
  objects.append(str(obj))
 exe=d/'hybrid-test'
 subprocess.run(['gcc',*flags,str(source),*objects,'-Wl,--gc-sections','-lm','-o',str(exe)],cwd=ROOT,check=True)
 subprocess.run([str(exe)],cwd=ROOT,check=True,timeout=60)
