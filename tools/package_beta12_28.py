#!/usr/bin/env python3
"""Package only tracked source and synthetic verification; never the SD tree.

Build inputs are fresh git archives. Reports directory must contain only the
explicit synthetic records named below, not user runtime logs or profiles.
"""
import argparse,hashlib,io,json,subprocess,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build-dir',type=Path,required=True)
p.add_argument('--reports-dir',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--build-commit',required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
release='R1389-SPEED-BETA12.28-ALPHA-CORE'
def sha(b):return hashlib.sha256(b).hexdigest()
def git(*args):return subprocess.check_output(['git',*args],cwd=root)
source_commit=git('rev-parse','HEAD').decode().strip()
subprocess.run(['git','diff','--exit-code',a.build_commit,source_commit,'--',
                'source','include','Makefile','tools/build_beta12_28_alpha.sh'],cwd=root,check=True)
subprocess.run(['git','diff','--exit-code','--','source','include','Makefile'],cwd=root,check=True)
files={}
for suffix in ('GX-Chain','GX'):
 for ext in ('dol','elf'):
  name=f'Gekko2-BETA12.28-{suffix}.{ext}';data=(a.build_dir/name).read_bytes()
  assert release.encode()+b'\0' in data,name+' has wrong embedded version'
  assert b'R1389-SPEED-BETA12.27' not in data,name+' contains stale version'
  files[name]=data
records=['beta12-28-host.json','options-final.json','memory-chain.json','memory-control.json',
         'dol-chain.json','dol-control.json','linked-results.json','chain-comparison.json','allocator.txt',
         'allocator-control.txt','chain-abi.txt','native-abi.txt','risk-profiles.txt',
         'wii-build-chain.txt','wii-build-control.txt','compile-flags.json','pull-requests.json']
reports={name:(a.reports_dir/name).read_bytes() for name in records}
host=json.loads(reports['beta12-28-host.json']);assert len(host)==62 and all(x['exit_code']==0 for x in host)
linked=json.loads(reports['linked-results.json']);assert linked and all(x['exit_code']==0 for x in linked)
for name in ('memory-chain.json','memory-control.json'):assert json.loads(reports[name])['passed']
for name in ('dol-chain.json','dol-control.json'):assert json.loads(reports[name])['elf_section_bytes_match']
for name in ('allocator.txt','allocator-control.txt','chain-abi.txt','native-abi.txt'):assert reports[name].lstrip().startswith((b'PASS',b'WORD_ALLOC'))
manifest={'release':release,'source_commit':source_commit,'emulator_build_commit':a.build_commit,
 'sdk':'devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48',
 'devkitPPC':'r50','gcc':'16.1.0','libogc':'3.1.0','unicorn':'2.1.4',
 'default_experiments':{'chain':False,'psm':False,'gx_risk':False},
 'private_aot_entries':0,'hardware_boot_verified':False,'gameplay_verified':False,
 'memory':{x:json.loads(reports['memory-'+x+'.json']) for x in ('chain','control')},
 'dynamic_card_bytes':8650784,'dynamic_peak_mem2_verified':False,
 'binaries':{n:{'bytes':len(b),'sha256':sha(b)} for n,b in files.items()},
 'compile_flags':json.loads(reports['compile-flags.json'])}
source=git('archive','--format=zip','--prefix=Gekko2-source/',source_commit)
with zipfile.ZipFile(io.BytesIO(source)) as z:
 for n in z.namelist():
  # Existing CHD-reader fixtures are synthetic and explicitly allowlisted.
  if n.endswith(('.iso','.chd')):assert '/tests/fixtures/chd-r1378/' in n,n
  assert not n.endswith(('.ps2','.rom','.key','.state','.log')),n
readme='''Gekko2 BETA12.28 – Entwicklungs-Teststand

apps/Gekko2 auf SD kopieren. Vorhandene Einstellungen werden nicht geliefert.
Eigene rechtmäßige BIOS/Disc-Dateien verwenden. Alle Experimente ab Werk OFF;
gespeicherte frühere Einstellungen bleiben erhalten. Änderungen: Settings,
Experiments/Developer switches; nächster Cold Boot übernimmt die Auswahl.

Optional vorhandene Karte: sd:/pcsx2/memcards/MemoryCard1.ps2.
Keine automatische Erstellung/Formatierung. Der emulierte FORMAT-Befehl
formatiert jetzt tatsächlich; Karte und Daten werden niemals mitgeliefert.

Kontroll-DOL ohne Chain-Unterstützung, ELF, Source und Prüfungen im Review-ZIP.
Das ist kein Nachweis von Crash/Tekken-Spielbarkeit, korrigierten BIOS-Kacheln
oder höherer Wii-FPS. Vollständigen Bericht im Review-ZIP lesen.
'''.encode()
def archive(path,items):
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for n,b in items.items():
   entry=zipfile.ZipInfo(n,(2026,10,17,0,0,0));entry.compress_type=zipfile.ZIP_DEFLATED
   entry.external_attr=0o100644<<16;z.writestr(entry,b)
manifest_bytes=(json.dumps(manifest,indent=2)+'\n').encode()
sd={'apps/Gekko2/boot.dol':files['Gekko2-BETA12.28-GX-Chain.dol'],
    'apps/Gekko2/meta.xml':(root/'meta.xml').read_bytes(),'apps/Gekko2/icon.png':(root/'assets/branding/icon.png').read_bytes(),
    'Gekko2-BETA12.28-README.txt':readme,'Gekko2-BETA12.28-manifest.json':manifest_bytes}
archive(a.output/'Gekko2-BETA12.28-SD.zip',sd)
review={**{'binaries/'+n:b for n,b in files.items()},**{'verification/'+n:b for n,b in reports.items()},
        'Gekko2-BETA12.28-source.zip':source,'manifest.json':manifest_bytes,'README.txt':readme,
        'BUILD-REPORT.md':(root/'docs/BETA12.28-ALPHA-CORE.md').read_bytes(),
        'IMPLEMENTATION.md':(root/'docs/ALPHA-UMSETZUNG.md').read_bytes()}
archive(a.output/'Gekko2-BETA12.28-review.zip',review)
(a.output/'manifest.json').write_bytes(manifest_bytes)
(a.output/'BUILD-REPORT.md').write_bytes(review['BUILD-REPORT.md'])
sums=[]
for n in ('Gekko2-BETA12.28-SD.zip','Gekko2-BETA12.28-review.zip'):
 b=(a.output/n).read_bytes();sums.append(f'{sha(b)}  {n}');print(n,len(b),sha(b))
(a.output/'SHA256SUMS.txt').write_text('\n'.join(sums)+'\n')
