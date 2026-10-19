#!/usr/bin/env python3
"""Completion batches plus full BETA12.28 synthetic regression baseline."""
from pathlib import Path
import argparse,json,os,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output-json',type=Path,required=True);a=p.parse_args()
env=dict(os.environ)
env['HOSTEXTRA']=env.get('HOSTEXTRA','')+' -DGEKKO2_GAME_TRACE -DGEKKO2_GX_EXTENDED_TRIANGLES -DGEKKO2_GX_GENERIC_SHADER -DGEKKO2_GX_NATIVE_BIOS_TRI -DGEKKO2_GX_RISK_MODE'
names=['test_gs_irq_access','test_gs_lod_mtba','test_vif_cpu_fifo','test_spu2_envx',
       'test_gs_mipmap','test_gs_mipmap_triangle','test_gs_context2_mipmap','test_vif_micro_barriers']
with tempfile.TemporaryDirectory() as directory:
 base=Path(directory)/'base.json'
 subprocess.run([sys.executable,'tools/test_beta12_29_host.py','--output-json',str(base)],cwd=root,env=env,check=True)
 rows=json.loads(base.read_text())
 paths=subprocess.check_output([sys.executable,'tools/build_host_core.py',*names],cwd=root,env=env,text=True).splitlines()
 assert len(paths)==len(names)
 for name,path in zip(names,paths):
  r=subprocess.run([path],cwd=root,capture_output=True,text=True)
  rows.append(dict(test=name,exit_code=r.returncode,output=r.stdout+r.stderr))
  print(('PASS ' if r.returncode==0 else 'FAIL ')+name+'\n'+r.stdout+r.stderr,flush=True)
a.output_json.parent.mkdir(parents=True,exist_ok=True);a.output_json.write_text(json.dumps(rows,indent=2)+'\n')
assert len(rows)==78 and all(r['exit_code']==0 for r in rows)
print('PASS 78 host suites; no hardware/GPU accuracy or Wii speed claim')
