#!/usr/bin/env python3
"""Current 83 host gates plus four EE/GS/IRX regressions and driver options.
Only synthetic inputs. No Wii cycles, physical GX pixels or gameplay claim.
"""
import argparse,json,os,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output-json',type=Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='gekko2-alpha-host-') as td:
 base=Path(td)/'base.json'
 subprocess.run([sys.executable,'tools/test_beta12_31_host.py','--output-json',str(base)],cwd=root,check=True)
 rows=json.loads(base.read_text())
 names=['test_ee_masked_window','test_gs_textured_point','test_iop_reloc_variants','test_gs_textured_line','game_check_r1386']
 built=subprocess.check_output([sys.executable,'tools/build_host_core.py',*names],cwd=root,text=True).splitlines()
 assert len(built)==len(names)
 for name,exe in zip(names[:-1],built[:-1]):
  r=subprocess.run([exe],cwd=root,capture_output=True,text=True,timeout=90)
  rows.append(dict(test=name,exit_code=r.returncode,output=r.stdout+r.stderr));print(name,r.returncode,r.stdout,flush=True)
 errors=[]
 for mask in ['0','1','02000000','00000000x','-1','100000000','']:
  r=subprocess.run([built[-1],str(Path(td)/'nonexistent-synthetic-bios'),'-','1'],cwd=root,
      env=dict(os.environ,GEKKO2_OPTIONS=mask),capture_output=True,text=True,timeout=10)
  valid=mask in ['0','1','02000000']
  if valid:
   ok=r.returncode==3 and ('OPTIONS mask=%08x forced_scheduler_bits=0'%int(mask,16)) in r.stdout
  else:ok=r.returncode==2 and 'Invalid GEKKO2_OPTIONS' in r.stderr
  if not ok:errors.append(mask)
 rows.append(dict(test='diagnostic_option_isolation',exit_code=bool(errors),output='seven synthetic missing-file option cases; errors='+repr(errors)))
a.output_json.parent.mkdir(parents=True,exist_ok=True);a.output_json.write_text(json.dumps(rows,indent=2)+'\n')
assert len(rows)==88 and all(r['exit_code']==0 for r in rows)
print('PASS 88 host gates; no Wii/gameplay evidence')
