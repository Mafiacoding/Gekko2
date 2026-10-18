#!/usr/bin/env python3
"""Package only tracked source, built binaries and explicit synthetic records."""
import argparse,hashlib,json,subprocess,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--reports',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--build-commit',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1]
def git(*args):return subprocess.check_output(['git',*args],cwd=r)
source=git('rev-parse','HEAD').decode().strip();build=git('rev-parse',a.build_commit).decode().strip()
git('diff','--exit-code',build,source,'--','source','include','Makefile','tools/windows-telemetry','tools/build_beta12_31c_network_menu.sh')
git('diff','--exit-code','--','source','include','Makefile','tools/windows-telemetry')
records={n:(a.reports/n).read_bytes() for n in ('host-network.txt','host-status.json','host-work.txt','host-menu.txt','linked-worker.txt','linked-network.txt','dol.json','memory.json','wii-build.txt')}
assert 'PASS telemetry default-off/' in records['host-network.txt'].decode() and 'PASS C/Python wire interoperability' in records['host-network.txt'].decode()
host=json.loads(records['host-status.json']);assert len(host)==4 and all(row['exit_code']==0 for row in host)
assert records['host-work.txt'].startswith(b'PASS real EE retirement')
assert records['host-menu.txt'].startswith(b'PASS network menu entry')
assert records['linked-worker.txt'].startswith(b'PASS 49 actual linked PPC')
assert records['linked-network.txt'].startswith(b'PASS 33 actual linked PPC')
assert json.loads(records['dol.json'])['elf_section_bytes_match']
memory=json.loads(records['memory.json']);assert memory['passed']
binaries={ext:(r/('Gekko2-BETA12.31c-GX-Chain.'+ext)).read_bytes() for ext in ('dol','elf')}
for b in binaries.values():assert b'R1389-SPEED-BETA12.31c-NETWORK-SETUP\0' in b and build.encode() in b
manifest=dict(release='BETA12.31c-NETWORK-SETUP',build_commit=build,source_commit=source,
 sdk='devkitPPC r50 GCC16.1.0 / libogc3.1.0',targeted_menu_config_suites=4,linked_ppc_wire_checks=33,linked_ppc_worker_api_checks=49,
 host_network_loopback_verified=True,real_profile_work_verified=True,memory=memory,
 physical_wii_network_verified=False,windows_native_verified=False,dolphin_verified=False,
 gx_fault_fixed=False,bios_tiles_fixed=False,fps_gain_verified=False,telemetry_factory_default='OFF',
 binaries={ext:dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest()) for ext,b in binaries.items()})
m=(json.dumps(manifest,indent=2)+'\n').encode();report=(r/'docs/BETA12.31c-NETWORK-SETUP.md').read_bytes()
a.output.mkdir(parents=True,exist_ok=True)
def archive(name,entries):
 with zipfile.ZipFile(a.output/name,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in entries.items():z.writestr(n,b)
 with zipfile.ZipFile(a.output/name) as z:assert z.testzip() is None
windows={n:(r/'tools/windows-telemetry'/n).read_bytes() for n in ('receiver.py','SETUP-SD.cmd','START-LOGGER.cmd','README-DE.md')}
archive('Gekko2-Windows-Logger-1.1.zip',{'Windows-Logger/'+n:b for n,b in windows.items()})
archive('Gekko2-BETA12.31c-SD.zip',{'apps/Gekko2/boot.dol':binaries['dol'],
 'apps/Gekko2/meta.xml':(r/'meta.xml').read_bytes(),'apps/Gekko2/icon.png':(r/'assets/branding/icon.png').read_bytes(),
 'manifest.json':m,'BUILD-REPORT.md':report,'START-HERE-DE.md':windows['README-DE.md'],
 **{'Windows-Logger/'+n:b for n,b in windows.items()}})
archive('Gekko2-BETA12.31c-review.zip',{'Gekko2-source.zip':git('archive','--format=zip',source),
 'binaries/Gekko2-BETA12.31c.dol':binaries['dol'],'binaries/Gekko2-BETA12.31c.elf':binaries['elf'],
 'manifest.json':m,'BUILD-REPORT.md':report,**{'verification/'+n:b for n,b in records.items()}})
(a.output/'manifest.json').write_bytes(m)
(a.output/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.name+'\n' for f in sorted(a.output.glob('*.zip'))))
print((a.output/'SHA256SUMS.txt').read_text())
