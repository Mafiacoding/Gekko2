#!/usr/bin/env python3
"""Compile the current host core once, then run selected executable tests."""
from pathlib import Path
import subprocess,sys,hashlib,concurrent.futures
root=Path(__file__).resolve().parents[1];out=root/'validation/r1384-host';out.mkdir(parents=True,exist_ok=True)
flags=['-O1','-w','-ffunction-sections','-fdata-sections','-Iinclude','-Isource','-Ithird_party/libchdr/include','-Ithird_party/libchdr/deps/lzma-25.01/include','-DWANT_RAW_DATA_SECTOR=1','-DWANT_SUBCODE=1','-DVERIFY_BLOCK_CRC=1']
sources=[p for p in (root/'source').rglob('*.c') if p.name not in ('main.c','ppc_dynarec.c')]
sources+=list((root/'third_party/libchdr/src').glob('*.c'))+list((root/'third_party/libchdr/deps/lzma-25.01/src').glob('*.c'))+list((root/'third_party/libchdr/deps/miniz-3.1.1').glob('*.c'))+list((root/'third_party/libchdr/deps/zstd-1.5.7').glob('*.c'))
stamp=max(x.stat().st_mtime for d in [root/"include",root/"source"] for x in d.rglob("*") if x.suffix in (".h",".inc"))
def compile(p):
 obj=out/(hashlib.sha256(str(p).encode()).hexdigest()[:16]+'.o')
 if not obj.exists() or obj.stat().st_mtime<max(p.stat().st_mtime,stamp):
  subprocess.run(['gcc',*flags,'-c',str(p),'-o',str(obj)],cwd=root,check=True)
 return p,obj
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=dict(pool.map(compile,sources))
for name in sys.argv[1:] or ['test_hardware_accuracy_r1384']:
 src=root/'tests'/f'{name}.c';exe=out/name
 import re
 excluded=set()
 for included in re.findall(r'#include "([^"]+\.c)"',src.read_text()):
  target=(src.parent/included).resolve() if included.startswith('.') else root/'source'/included
  if target.is_relative_to(root/'source'):excluded.add(str(target.relative_to(root/'source')))
 objs=[str(o) for p,o in objects.items() if not (p.is_relative_to(root/'source') and str(p.relative_to(root/'source')) in excluded)]
 subprocess.run(['gcc',*flags,str(src),*objs,'-Wl,--gc-sections','-o',str(exe),'-lm'],cwd=root,check=True)
 subprocess.run([str(exe)],cwd=root,check=True)
