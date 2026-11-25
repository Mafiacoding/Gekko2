#!/usr/bin/env python3
"""Build-gated BETA12.32 artifacts; synthetic evidence only, no owner uploads."""
import argparse,hashlib,json,re,subprocess,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--reports',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--build-commit',required=True);a=p.parse_args();root=Path(__file__).resolve().parents[1]
def git(*args):return subprocess.check_output(['git',*args],cwd=root)
build=git('rev-parse',a.build_commit).decode().strip();source=git('rev-parse','HEAD').decode().strip()
git('diff','--exit-code',build,source,'--','source','include','Makefile','tools/build_beta12_32_hotpaths.sh')
git('diff','--exit-code','--','source','include','Makefile','tools/windows-telemetry')
records={n:(a.reports/n).read_bytes() for n in ('host.json','final-linked.json','ee-admission-before.txt','ee-admission-after.txt','cache-before.txt','cache-after.txt','gx-stream-before.txt','gx-stream-after.txt','gx-stream-async.txt','gx-fence.txt','gx-stack.json','network-worker.txt','dol.json','memory.json','dol-control.json','memory-control.json','dol-async.json','memory-async.json','wii-build-async-control.txt','wii-build.txt','wii-build-control.txt')}
for filename,count in [('host.json',83),('final-linked.json',16)]:
 rows=json.loads(records[filename]);assert len(rows)==count and all(row['exit_code']==0 for row in rows)
for filename in ('dol.json','dol-control.json','dol-async.json'):assert json.loads(records[filename])['elf_section_bytes_match']
for filename in ('memory.json','memory-control.json','memory-async.json'):assert json.loads(records[filename])['passed']
def tagged(name,tag):return json.loads(next(l[len(tag):] for l in records[name].decode().splitlines() if l.startswith(tag)))
assert tagged('gx-stream-before.txt','GX_SEMANTICS ')==tagged('gx-stream-after.txt','GX_SEMANTICS ')
assert tagged('gx-stream-before.txt','GX_SUBMISSION_FENCES ')==0
assert tagged('gx-stream-after.txt','GX_SUBMISSION_FENCES ')>0
assert tagged('gx-stream-async.txt','GX_SUBMISSION_FENCES ')==0
for tag in ('GX_SEMANTICS ','GX_STRESS '):
 assert tagged('gx-stream-before.txt',tag)==tagged('gx-stream-after.txt',tag)==tagged('gx-stream-async.txt',tag)
assert tagged('gx-stream-before.txt','GX_QUEUE_PEAK ')==373
assert tagged('gx-stream-after.txt','GX_QUEUE_PEAK ')==64
assert tagged('gx-stream-async.txt','GX_QUEUE_PEAK ')==373
assert b'PASS per-call native submission coverage:' in records['gx-stream-after.txt']
assert b'PASS resident submission failure:' in records['gx-stream-after.txt']
assert b'PASS linked PPC GX fence:' in records['gx-fence.txt'] and b'PASS linked libogc UDP transport:' in records['network-worker.txt']
for n in ('ee-admission-before.txt','ee-admission-after.txt'):assert b'PASS six physical placements:' in records[n]
before=json.JSONDecoder().raw_decode(records['ee-admission-before.txt'].decode())[0];after=json.JSONDecoder().raw_decode(records['ee-admission-after.txt'].decode())[0]
assert before[0]['block_retired']==0 and after[0]['block_retired']==64
assert after[-1]['block_retired']==0
cache_before=tagged('cache-before.txt','BUDGET_RANGE_COST ');cache_after=tagged('cache-after.txt','BUDGET_RANGE_COST ')
assert cache_before['calls']==cache_after['calls']==5700 and cache_after['PPC_instructions']<cache_before['PPC_instructions']
binaries={}
for variant in ('GX-Chain','GX','GX-Chain-AsyncControl'):
 for ext in ('dol','elf'):
  n=f'Gekko2-BETA12.32-{variant}.{ext}';b=(root/n).read_bytes();assert b'R1389-SPEED-BETA12.32-HOTPATHS-GX\0' in b and build.encode() in b;binaries[n]=b
assert json.loads(records['gx-stack.json'])['after']['elf_sha256']==hashlib.sha256(binaries['Gekko2-BETA12.32-GX-Chain.elf']).hexdigest()
for row in json.loads(records['final-linked.json']):
 assert row['elf_sha256']==hashlib.sha256(binaries[row['elf']]).hexdigest()
manifest=dict(release='BETA12.32-HOTPATHS-GX',build_commit=build,source_commit=source,toolchain='devkitPPC r50 GCC16.1.0 / libogc3.1.0, O1, existing SPEED/DSI/AOT/GX flags',host_suites=83,linked_ppc_suites=16,synthetic_before=before,synthetic_after=after,cache_before=cache_before,cache_after=cache_after,gx_semantics=tagged('gx-stream-after.txt','GX_SEMANTICS '),gx_stack_frames=json.loads(records['gx-stack.json']),physical_wii_tested=False,wii_fps_gain_verified=False,gx_timeout_fixed=False,gx_submission_guard_coverage_fixed=True,native_begins_before_fence_before=373,native_begins_before_fence_after=64,experimental_defaults_unchanged=True,binaries={n:dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest()) for n,b in binaries.items()})
m=(json.dumps(manifest,indent=2)+'\n').encode();report=(root/'docs/BETA12.32-HOTPATHS-GX.md').read_bytes();a.output.mkdir(parents=True,exist_ok=True)
def archive(name,items):
 with zipfile.ZipFile(a.output/name,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in items.items():z.writestr(n,b)
 with zipfile.ZipFile(a.output/name) as z:assert z.testzip() is None
windows={n:(root/'tools/windows-telemetry'/n).read_bytes() for n in ('receiver.py','SETUP-SD.cmd','START-LOGGER.cmd','README-DE.md')}
archive('Gekko2-BETA12.32-SD.zip',{'apps/Gekko2/boot.dol':binaries['Gekko2-BETA12.32-GX-Chain.dol'],'apps/Gekko2/meta.xml':(root/'meta.xml').read_bytes(),'apps/Gekko2/icon.png':(root/'assets/branding/icon.png').read_bytes(),'manifest.json':m,'BUILD-REPORT.md':report,**{'Windows-Logger/'+n:b for n,b in windows.items()}})
archive('Gekko2-BETA12.32-review.zip',{'Gekko2-source.zip':git('archive','--format=zip',source),'manifest.json':m,'BUILD-REPORT.md':report,**{'binaries/'+n:b for n,b in binaries.items()},**{'verification/'+n:b for n,b in records.items()}})
(a.output/'manifest.json').write_bytes(m);(a.output/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.name+'\n' for f in sorted(a.output.glob('*.zip'))));print((a.output/'SHA256SUMS.txt').read_text())
