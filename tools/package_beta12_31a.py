#!/usr/bin/env python3
"""Package explicit CPU-status build outputs and synthetic reports only."""
import argparse,hashlib,json,subprocess,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--reports',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--build-commit',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1]
def git(*args):return subprocess.check_output(['git',*args],cwd=r)
commit=git('rev-parse','HEAD').decode().strip()
git('diff','--exit-code',a.build_commit,commit,'--','source','include','Makefile','tools/build_beta12_31a_status.sh')
git('diff','--exit-code','--','source','include','Makefile')
host=json.loads((a.reports/'host-tests.json').read_text());assert len(host)==4 and all(x['exit_code']==0 for x in host)
assert json.loads((a.reports/'dol.json').read_text())['elf_section_bytes_match']
memory=json.loads((a.reports/'memory.json').read_text());assert memory['passed']
binaries={ext:(r/('Gekko2-BETA12.31a-GX-Chain.'+ext)).read_bytes() for ext in ('dol','elf')}
for b in binaries.values():assert b'R1389-SPEED-BETA12.31a-CPU-STATUS\0' in b
manifest=dict(release='BETA12.31a-CPU-STATUS',build_commit=a.build_commit,source_commit=commit,
 targeted_host_suites=4,sdk='devkitPPC r50 GCC16.1.0 / libogc3.1.0',memory=memory,
 physical_wii_verified=False,dolphin_verified=False,gx_fault_fixed=False,bios_tiles_fixed=False,fps_gain_verified=False,
 binaries={ext:dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest()) for ext,b in binaries.items()})
m=(json.dumps(manifest,indent=2)+'\n').encode();report=(r/'docs/BETA12.31a-CPU-STATUS.md').read_bytes()
a.output.mkdir(parents=True,exist_ok=True)
def archive(name,entries):
 with zipfile.ZipFile(a.output/name,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in entries.items():z.writestr(n,b)
 with zipfile.ZipFile(a.output/name) as z:assert z.testzip() is None
archive('Gekko2-BETA12.31a-SD.zip',{'apps/Gekko2/boot.dol':binaries['dol'],
 'apps/Gekko2/meta.xml':(r/'meta.xml').read_bytes(),'apps/Gekko2/icon.png':(r/'assets/branding/icon.png').read_bytes(),
 'manifest.json':m,'BUILD-REPORT.md':report})
archive('Gekko2-BETA12.31a-review.zip',{'Gekko2-source.zip':git('archive','--format=zip',commit),
 'binaries/Gekko2-BETA12.31a.dol':binaries['dol'],'binaries/Gekko2-BETA12.31a.elf':binaries['elf'],
 'manifest.json':m,'BUILD-REPORT.md':report,**{'verification/'+n:(a.reports/n).read_bytes() for n in ('host-tests.json','dol.json','memory.json','wii-build.txt')}})
(a.output/'manifest.json').write_bytes(m)
(a.output/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.name+'\n' for f in sorted(a.output.glob('*.zip'))))
print((a.output/'SHA256SUMS.txt').read_text())
