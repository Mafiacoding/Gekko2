#!/usr/bin/env python3
"""Generate synthetic VU AOT mask fixtures outside tracked source and compare pipelines."""
import importlib.util,subprocess,tempfile,hashlib,shlex
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('generate',root/'tools/vu_aot_generate.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
words=[]
for relative in (0,1):
 for store in (0,1):
  for mask in range(16):words.append(((0x800003fe|store) if relative else ((4+store)<<25)|1)|2<<16|1<<11|mask<<21)
upper=(15<<21)|(2<<16)|(1<<11)|(3<<6)|40
with tempfile.TemporaryDirectory(prefix='gekko23-vu-aot-') as directory:
 d=Path(directory);(d/'hw').mkdir();(d/'hw/vu_aot_generated.inc').write_text(m.generate([upper],words))
 (d/'vu_aot_test_fixture.h').write_text('static const unsigned vu_aot_fixture_count=64;\nstatic const unsigned vu_aot_fixtures[64][2]={'+','.join('{'+str(upper)+','+str(w)+'}' for w in words)+'};\n')
 # Reuse ordinary host objects; only the VU translation unit gets synthetic AOT.
 subprocess.run(['python3','tools/build_host_core.py','test_vu_integer_memory_masks'],cwd=root,check=True,stdout=subprocess.DEVNULL)
 flags=['-O3','-w','-ffunction-sections','-fdata-sections','-Iinclude','-Isource','-Ithird_party/libchdr/include','-Ithird_party/libchdr/deps/lzma-25.01/include','-DWANT_RAW_DATA_SECTOR=1','-DWANT_SUBCODE=1','-DVERIFY_BLOCK_CRC=1']
 cache=root/'validation/r1386-host'/hashlib.sha256(('gcc\0'+'\0'.join(flags)).encode()).hexdigest()[:16]
 vuobj=hashlib.sha256(str(root/'source/hw/vu.c').encode()).hexdigest()[:16]+'.o'
 objects=[str(p) for p in cache.glob('*.o') if p.name!=vuobj]
 exe=d/'test'
 subprocess.run(['gcc','-I'+str(d),*flags,'-Isource/hw','-DGEKKO2_VU_AOT','tests/test_vu_aot.c','source/hw/vu.c',*objects,'-Wl,--gc-sections','-lm','-o',str(exe)],cwd=root,check=True)
 subprocess.run([str(exe)],cwd=root,check=True)
