#!/usr/bin/env python3
"""Synthetic Alpha-core integration; no game, GPU or Wii-speed claim."""
from pathlib import Path
import argparse,json,os,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output-json',type=Path,required=True);a=p.parse_args()
env=dict(os.environ)
env['HOSTEXTRA']=env.get('HOSTEXTRA','')+' -DGEKKO2_GAME_TRACE -DGEKKO2_GX_EXTENDED_TRIANGLES -DGEKKO2_GX_GENERIC_SHADER -DGEKKO2_GX_NATIVE_BIOS_TRI -DGEKKO2_GX_RISK_MODE'
names=['test_alpha_settings','test_alpha_clut','test_alpha_scanout',
       'test_ps2_memory_card','test_ps2_mc_checkpoint','test_ps2_mc_files','test_ipu_cpu_fifo']
with tempfile.TemporaryDirectory() as directory:
 base=Path(directory)/'base.json'
 subprocess.run([sys.executable,'tools/test_beta12_27_host.py','--output-json',str(base)],cwd=root,env=env,check=True)
 rows=json.loads(base.read_text())
 paths=subprocess.check_output([sys.executable,'tools/build_host_core.py',*names],cwd=root,env=env,text=True).splitlines()
 assert len(paths)==len(names)
 for name,path in zip(names,paths):
  r=subprocess.run([path],cwd=root,capture_output=True,text=True)
  rows.append(dict(test=name,exit_code=r.returncode,output=r.stdout+r.stderr))
  print(('PASS ' if r.returncode==0 else 'FAIL ')+name+'\n'+r.stdout+r.stderr,flush=True)
a.output_json.parent.mkdir(parents=True,exist_ok=True);a.output_json.write_text(json.dumps(rows,indent=2)+'\n')
assert len(rows)==62 and all(r['exit_code']==0 for r in rows)
print('PASS 62 host suites; hardware rasterization, gameplay and Wii speed unverified')
