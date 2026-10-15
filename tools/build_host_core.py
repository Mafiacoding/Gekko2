#!/usr/bin/env python3
"""Compile the current host core once, then run selected executable tests."""
from pathlib import Path
import subprocess,sys,hashlib,concurrent.futures,re
root=Path(__file__).resolve().parents[1];out=root/'validation/r1386-host';out.mkdir(parents=True,exist_ok=True)
flags=['-O3','-w','-ffunction-sections','-fdata-sections','-Iinclude','-Isource','-Ithird_party/libchdr/include','-Ithird_party/libchdr/deps/lzma-25.01/include','-DWANT_RAW_DATA_SECTOR=1','-DWANT_SUBCODE=1','-DVERIFY_BLOCK_CRC=1']+__import__('shlex').split(__import__('os').environ.get('HOSTEXTRA',''))
# Distinct compiler flags must never silently reuse objects from another mode.
out=out/hashlib.sha256(('gcc\0'+'\0'.join(flags)).encode()).hexdigest()[:16]
out.mkdir(parents=True,exist_ok=True)
sources=[p for p in (root/'source').rglob('*.c') if p.name not in ('main.c','ppc_dynarec.c')]
sources+=list((root/'third_party/libchdr/src').glob('*.c'))+list((root/'third_party/libchdr/deps/lzma-25.01/src').glob('*.c'))+list((root/'third_party/libchdr/deps/miniz-3.1.1').glob('*.c'))+list((root/'third_party/libchdr/deps/zstd-1.5.7').glob('*.c'))
stamp=max(x.stat().st_mtime for d in [root/"include",root/"source"] for x in d.rglob("*") if x.suffix in (".h",".inc"))
def compile(p):
 obj=out/(hashlib.sha256(str(p).encode()).hexdigest()[:16]+'.o')
 if not obj.exists() or obj.stat().st_mtime<max(p.stat().st_mtime,stamp):
  subprocess.run(['gcc',*flags,'-c',str(p),'-o',str(obj)],cwd=root,check=True)
 return p,obj
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=dict(pool.map(compile,sources))
for name in sys.argv[1:]:
 src=root/'tools'/f'{name}.c';
 if not src.is_file():src=root/'tests'/f'{name}.c'
 exe=out/name
 # White-box tests include the .c file under test; never link the
 # same translation unit again from the prebuilt object cache.
 # Included source can be nested through another test fixture.
 def included_sources(file,seen=None):
  seen=set() if seen is None else seen
  if file in seen:return set()
  seen.add(file)
  result=set()
  for inc in re.findall(r'#include\s+"([A-Za-z0-9_./-]+\.c)"',file.read_text()):
   if inc.startswith(('core/','hw/')):result.add(inc)
   elif (file.parent/inc).is_file():result.update(included_sources(file.parent/inc,seen))
  return result
 included=included_sources(src)
 link_objects=[str(obj) for p,obj in objects.items() if not (p.is_relative_to(root/'source') and str(p.relative_to(root/'source')) in included)]
 subprocess.run(['gcc',*flags,str(src),*link_objects,'-Wl,--gc-sections','-o',str(exe),'-lm'],cwd=root,check=True)
 print(exe)
