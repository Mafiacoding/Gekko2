#!/usr/bin/env python3
"""Prepare a private SD review candidate from explicitly gated build records.
This creates no GitHub release and reads no owner capture/BIOS/disc files.
"""
import argparse,hashlib,json,subprocess,zipfile
import xml.etree.ElementTree as ET
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--reports',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--build-commit',required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1]
def git(*args):return subprocess.check_output(['git',*args],cwd=root)
def sha(data):return hashlib.sha256(data).hexdigest()
build=git('rev-parse',a.build_commit).decode().strip()
source=git('rev-parse','HEAD').decode().strip()
runtime=['source','include','Makefile','tools/build_beta12_33_windows.sh']
git('diff','--exit-code',build,source,'--',*runtime)
git('diff','--exit-code','--',*runtime)
out=a.output.resolve();assert out!=root and root not in out.parents,'keep review outputs outside checkout'
# Exact names only: the reports directory may also contain private captures.
host=json.loads((a.reports/'host.json').read_text())
linked=json.loads((a.reports/'linked.json').read_text())
assert len(host)==90 and all(r['exit_code']==0 for r in host)
assert len(linked)==6 and all(r['exit_code']==0 for r in linked)
required={'verify_cdvd_error_rpc_linked.py','verify_masked_window_linked.py','verify_event_scheduler_r1353.py'}
binaries={};gates={}
for variant,label in [('GX','control'),('GX-Chain','chain')]:
 name='Gekko2-BETA12.33-'+variant
 dol=json.loads((a.reports/('dol-'+label+'.json')).read_text())
 memory=json.loads((a.reports/('memory-'+label+'.json')).read_text())
 assert dol['elf_section_bytes_match'] and memory['passed']
 # Re-run against the selected current files; stale report booleans are insufficient.
 subprocess.run(['python3','tools/verify_dol_r1352.py',name+'.dol',name+'.elf'],cwd=root,check=True,stdout=subprocess.DEVNULL)
 subprocess.run(['python3','tools/verify_wii_memory.py',name+'.elf'],cwd=root,check=True,stdout=subprocess.DEVNULL)
 elf=(root/(name+'.elf')).read_bytes();data=(root/(name+'.dol')).read_bytes()
 assert build.encode() in data and build.encode() in elf
 rows=[r for r in linked if r['elf']==name+'.elf']
 assert len(rows)==3 and {r['test'] for r in rows}==required
 assert all(r['elf_sha256']==sha(elf) for r in rows)
 binaries[name+'.dol']=dict(sha256=sha(data),bytes=len(data))
 gates[label]=dict(memory=memory,elf_sha256=sha(elf))
# The control build omits chain-residency compilation; no experimental mode
# is enabled by this package. Existing user SD settings are not overwritten.
selected='Gekko2-BETA12.33-GX.dol';data=(root/selected).read_bytes()
manifest=dict(kind='private-review-candidate',public_alpha_released=False,
 build_commit=build,source_commit=source,selected_dol=selected,
 toolchain='devkitPPC r50 GCC16.1.0 / libogc3.1.0; retained SPEED/AOT/DSI/GX fence flags',
 host_groups=90,linked_ppc_groups=6,physical_wii_tested=False,
 gameplay_verified=False,wii_fps_gain_verified=False,bios_tile_fix_verified=False,
 included_owner_assets=False,binaries=binaries,gates=gates)
meta=ET.fromstring((root/'meta.xml').read_bytes())
meta.find('version').text='BETA12.33 REVIEW '+build[:7]
meta.find('short_description').text='Technischer Pruefkandidat; Spielbarkeit unbestaetigt'
readme=('Gekko2: privater Alpha-Pruefkandidat '+build[:7]+'\n\n'
 'Kein oeffentlich freigegebener Alpha-Build. Keine bestaetigte Spielbarkeit,\n'
 'FPS-Steigerung oder BIOS-Kachelkorrektur. Der Build bleibt BETA12.33.\n\n'
 'Vor dem Kopieren vorhandene boot.dol und Einstellungen sichern.\n'
 'Dieses Paket enthaelt und ueberschreibt keine BIOS-, Disc- oder Kartendaten\n'
 'und schreibt keine Optionsdatei. Bestehende Einstellungen gelten weiterhin.\n'
 'Zum Abnahmetest Event-Scheduler, PSM-Konvertierung und GX-Risiko deaktivieren;\n'
 '128-Bit-Kettenresidenz ist in diesem Kontrollbuild nicht einkompiliert.\n\n'
 'Wii-Abnahme: HBC-Start, Controller, Einstellungen speichern, Cold Boot,\n'
 'erste BIOS-Ausgabe, Rueckkehr ins Menue und HBC. Release-ID, Build-ID und\n'
 'aktive Optionen im Log kontrollieren. Der Agent hat keine echte Wii getestet.\n\n'
 'GT2000-Demo erreicht im untersuchten Hostpfad noch keine Bildausgabe.\n'
 'GT3-Vollversion, Crash und Tekken sind nicht als spielbar bestaetigt.\n')
items={'apps/Gekko2/boot.dol':data,
 'apps/Gekko2/meta.xml':ET.tostring(meta,encoding='utf-8',xml_declaration=True),
 'apps/Gekko2/icon.png':(root/'assets/branding/icon.png').read_bytes(),
 'README-DE.txt':readme.encode(),
 'ALPHA-STAND.md':(root/'docs/ALPHA-READINESS-2026-10-19.md').read_bytes(),
 'manifest.json':(json.dumps(manifest,indent=2)+'\n').encode()}
out.mkdir(parents=True,exist_ok=True);target=out/('Gekko2-Alpha-Review-'+build[:7]+'-SD.zip')
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
 for name,body in items.items():z.writestr(name,body)
with zipfile.ZipFile(target) as z:
 assert z.testzip() is None and set(z.namelist())==set(items)
 assert z.read('apps/Gekko2/boot.dol')==data
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(out/'SHA256SUMS.txt').write_text(sha(target.read_bytes())+'  '+target.name+'\n')
print('Verified private review package:',target.name,sha(target.read_bytes()))
