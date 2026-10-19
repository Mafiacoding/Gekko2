#!/usr/bin/env python3
"""Package tracked source and explicit synthetic records; never a user SD tree."""
import argparse,hashlib,io,json,subprocess,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser()
p.add_argument('--build-dir',type=Path,required=True);p.add_argument('--benchmark-dir',type=Path,required=True);p.add_argument('--reports-dir',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--build-commit',required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
def git(*args):return subprocess.check_output(['git',*args],cwd=root)
def sha(b):return hashlib.sha256(b).hexdigest()
source=git('rev-parse','HEAD').decode().strip()
subprocess.run(['git','diff','--exit-code',a.build_commit,source,'--','source','include','apps','Makefile','tools/build_beta12_31_profile.sh','tools/build_wii_limits.sh'],cwd=root,check=True)
subprocess.run(['git','diff','--exit-code','--','source','include','apps','Makefile'],cwd=root,check=True)
records=['host-83.json','host-83.txt','linked-16.json','linked-16.txt','release-results.json',
         'wii-build-chain.txt','wii-build-control.txt','memory-chain.json','memory-control.json',
         'dol-chain.json','dol-control.json','compile-flags.json']
reports={n:(a.reports_dir/n).read_bytes() for n in records}
host=json.loads(reports['host-83.json']);linked=json.loads(reports['linked-16.json'])
assert len(host)==83 and all(x['exit_code']==0 for x in host)
assert len(linked)==16 and all(x['exit_code']==0 for x in linked)
for label in ('chain','control'):
 assert json.loads(reports['memory-'+label+'.json'])['passed']
 assert json.loads(reports['dol-'+label+'.json'])['elf_section_bytes_match']
assert 'PASS 87 ' in (a.benchmark_dir/'host-tests.txt').read_text()
assert 'PASS 135 ' in (a.benchmark_dir/'linked-tests.txt').read_text()
files={}
for variant in ('GX-Chain','GX'):
 for ext in ('dol','elf'):
  n=f'Gekko2-BETA12.31-{variant}.{ext}';b=(a.build_dir/n).read_bytes();assert b'R1389-SPEED-BETA12.31-RUNTIME-PROFILE\0' in b;files[n]=b
for ext in ('dol','elf'):
 n='Wii-Limits.'+ext;b=(a.benchmark_dir/n).read_bytes();assert b'GEKKO2-WII-LIMITS-1.1\0' in b;files[n]=b
assert (a.benchmark_dir/'build-commit.txt').read_text().strip().startswith(a.build_commit)
manifest={'release':'R1389-SPEED-BETA12.31-RUNTIME-PROFILE','benchmark':'GEKKO2-WII-LIMITS-1.1','source_commit':source,'build_commit':a.build_commit,
 'sdk':'devkitPPC r50/GCC16.1.0/libogc3.1.0','host_suites':83,'linked_ppc_suites':16,
 'benchmark_host_checks':87,'benchmark_linked_checks':135,'physical_wii_verified':False,'dolphin_verified':False,'fps_gain_verified':False,'bios_tile_fix_verified':False,
 'binaries':{n:{'bytes':len(b),'sha256':sha(b)} for n,b in files.items()},
 'memory':{x:json.loads(reports['memory-'+x+'.json']) for x in ('chain','control')}}
m=(json.dumps(manifest,indent=2)+'\n').encode();(a.output/'manifest.json').write_bytes(m)
def archive(name,entries):
 with zipfile.ZipFile(a.output/name,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for n,b in entries.items():
   entry=zipfile.ZipInfo(n,(2026,10,18,0,0,0));entry.compress_type=zipfile.ZIP_DEFLATED;entry.external_attr=0o100644<<16;z.writestr(entry,b)
 with zipfile.ZipFile(a.output/name) as z:assert z.testzip() is None
icon=(root/'assets/branding/icon.png').read_bytes()
bench={'apps/gekko2-wii-limits/boot.dol':files['Wii-Limits.dol'],'apps/gekko2-wii-limits/meta.xml':(root/'apps/wii_limits/meta.xml').read_bytes(),'apps/gekko2-wii-limits/icon.png':icon,'WII-LIMITS-README.md':(root/'apps/wii_limits/README.md').read_bytes()}
archive('Gekko2-Wii-Limits-1.1-SD.zip',{**bench,'manifest.json':m})
sd={**bench,'apps/Gekko2/boot.dol':files['Gekko2-BETA12.31-GX-Chain.dol'],'apps/Gekko2/meta.xml':(root/'meta.xml').read_bytes(),'apps/Gekko2/icon.png':icon,'manifest.json':m,'BUILD-REPORT.md':(root/'docs/BETA12.31-RUNTIME-PROFILE.md').read_bytes()}
archive('Gekko2-BETA12.31-SD.zip',sd)
sourcezip=git('archive','--format=zip','--prefix=Gekko2-source/',source)
with zipfile.ZipFile(io.BytesIO(sourcezip)) as z:
 for n in z.namelist():
  if n.endswith(('.iso','.chd')):assert '/tests/fixtures/chd-r1378/' in n
  assert not n.endswith(('.ps2','.rom','.key','.state','.log','.private')),n
review={**{'binaries/'+n:b for n,b in files.items()},**{'verification/'+n:b for n,b in reports.items()},'manifest.json':m,'Gekko2-source.zip':sourcezip,'BUILD-REPORT.md':(root/'docs/BETA12.31-RUNTIME-PROFILE.md').read_bytes()}
for n in ('host-tests.txt','linked-tests.txt','dol.json','memory.json','flags.txt','compiler.txt','build-commit.txt'):review['verification/benchmark/'+n]=(a.benchmark_dir/n).read_bytes()
archive('Gekko2-BETA12.31-review.zip',review)
(a.output/'BUILD-REPORT.md').write_bytes(sd['BUILD-REPORT.md'])
names=('Gekko2-Wii-Limits-1.1-SD.zip','Gekko2-BETA12.31-SD.zip','Gekko2-BETA12.31-review.zip')
(a.output/'SHA256SUMS.txt').write_text(''.join(sha((a.output/n).read_bytes())+'  '+n+'\n' for n in names))
for n in names:print(n,(a.output/n).stat().st_size,sha((a.output/n).read_bytes()))
