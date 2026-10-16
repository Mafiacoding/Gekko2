#!/usr/bin/env python3
"""Synthetic host regression gates for extended GX triangle submission."""
from pathlib import Path
import argparse,json,os,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output-json',type=Path,required=True);a=p.parse_args()
env=dict(os.environ);env['HOSTEXTRA']=env.get('HOSTEXTRA','')+' -DGEKKO2_GAME_TRACE -DGEKKO2_GX_EXTENDED_TRIANGLES -DGEKKO2_GX_GENERIC_SHADER'
with tempfile.TemporaryDirectory() as directory:
 base=Path(directory)/'base.json'
 subprocess.run([sys.executable,'tools/test_beta12_25_host.py','--output-json',str(base)],cwd=root,env=env,check=True)
 rows=json.loads(base.read_text())
 names=['test_gx_generic_sources','test_gx_generic_admission']
 paths=subprocess.check_output([sys.executable,'tools/build_host_core.py',*names],cwd=root,env=env,text=True).splitlines()
 assert len(paths)==len(names)
 for name,path in zip(names,paths):
  r=subprocess.run([path],cwd=root,capture_output=True,text=True)
  rows.append(dict(test=name,exit_code=r.returncode,output=r.stdout+r.stderr))
  print(('PASS ' if r.returncode==0 else 'FAIL ')+name+'\n'+r.stdout+r.stderr,flush=True)
a.output_json.parent.mkdir(parents=True,exist_ok=True);a.output_json.write_text(json.dumps(rows,indent=2)+'\n')
assert len(rows)==54 and all(r['exit_code']==0 for r in rows)
print('PASS 54 host suites; GPU rasterization and Wii gameplay unverified')
