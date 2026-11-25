#!/usr/bin/env python3
"""Package only explicitly enumerated synthetic reports and verified binaries."""
import argparse, hashlib, json, subprocess, zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--reports',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--build-commit',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
def git(*args):return subprocess.check_output(['git',*args],cwd=root)
def digest(b):return hashlib.sha256(b).hexdigest()
build=git('rev-parse',a.build_commit).decode().strip();source=git('rev-parse','HEAD').decode().strip()
paths=['source','include','Makefile','tools/build_beta12_33_windows.sh']
git('diff','--exit-code',build,source,'--',*paths)
git('diff','--exit-code','--',*paths,'meta.xml','tools/windows-telemetry')
host=json.loads((a.reports/'host.json').read_text());linked=json.loads((a.reports/'linked.json').read_text())
assert len(host)==83 and all(r['exit_code']==0 for r in host)
assert len(linked)==21 and all(r['exit_code']==0 for r in linked)
records={n:(a.reports/n).read_bytes() for n in ['host.json','host.txt','linked.json','gx-before.txt','event-refresh-negative.txt','dol.json','memory.json','dol-control.json','memory-control.json','build.txt','build-control.txt']}
assert b'IOP deadline was never refreshed' in records['event-refresh-negative.txt']
for name in ['dol.json','dol-control.json']:assert json.loads(records[name])['elf_section_bytes_match']
for name in ['memory.json','memory-control.json']:assert json.loads(records[name])['passed']
def result(label):return next(r['output'] for r in linked if r['test']==label)
def tagged(output,tag):return json.loads(next(l[len(tag):] for l in output.splitlines() if l.startswith(tag)))
before=records['gx-before.txt'].decode();after=result('verify_gx_vertex_stream')
for tag in ['GX_SEMANTICS ','GX_STRESS ']:assert tagged(before,tag)==tagged(after,tag)
cost_before=tagged(before,'GX_NATIVE_CPU_WORK ');cost_after=tagged(after,'GX_NATIVE_CPU_WORK ')
assert cost_after['executed_ppc_instructions']<cost_before['executed_ppc_instructions']
binaries={}
for variant in ['GX-Chain','GX']:
 for ext in ['dol','elf']:
  name=f'Gekko2-BETA12.33-{variant}.{ext}';data=(root/name).read_bytes()
  assert b'R1389-SPEED-BETA12.33-GX-DEADLINES\0' in data and build.encode() in data
  binaries[name]=data
for row in linked:assert row['elf_sha256']==digest(binaries[row['elf']])
manifest=dict(release='BETA12.33-GX-DEADLINES',build_commit=build,source_commit=source,host_suites=len(host),linked_ppc_suites=len(linked),toolchain='devkitPPC r50 GCC16.1.0 / libogc3.1.0; O1; retained SPEED/DSI/AOT/GX flags',gx_before=cost_before,gx_after=cost_after,physical_wii_tested=False,wii_fps_gain_verified=False,bios_tile_fix_verified=False,event_scheduler_default=False,binaries={n:dict(bytes=len(b),sha256=digest(b)) for n,b in binaries.items()})
metadata=(json.dumps(manifest,indent=2)+'\n').encode();report=(root/'docs/BETA12.33-GX-DEADLINES.md').read_bytes()
a.output.mkdir(parents=True,exist_ok=True)
def archive(name,items):
 with zipfile.ZipFile(a.output/name,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in items.items():z.writestr(n,b)
 with zipfile.ZipFile(a.output/name) as z:assert z.testzip() is None
windows={n:(root/'tools/windows-telemetry'/n).read_bytes() for n in ['receiver.py','SETUP-SD.cmd','START-LOGGER.cmd','README-DE.md']}
archive('Gekko2-BETA12.33-SD.zip',{'apps/Gekko2/boot.dol':binaries['Gekko2-BETA12.33-GX-Chain.dol'],'apps/Gekko2/meta.xml':(root/'meta.xml').read_bytes(),'apps/Gekko2/icon.png':(root/'assets/branding/icon.png').read_bytes(),'manifest.json':metadata,'BUILD-REPORT.md':report,**{'Windows-Logger/'+n:b for n,b in windows.items()}})
archive('Gekko2-BETA12.33-review.zip',{'Gekko2-source.zip':git('archive','--format=zip',source),'manifest.json':metadata,'BUILD-REPORT.md':report,**{'binaries/'+n:b for n,b in binaries.items()},**{'verification/'+n:b for n,b in records.items()}})
(a.output/'manifest.json').write_bytes(metadata)
(a.output/'SHA256SUMS.txt').write_text(''.join(digest(f.read_bytes())+'  '+f.name+'\n' for f in sorted(a.output.glob('*.zip'))))
print((a.output/'SHA256SUMS.txt').read_text())
