#!/usr/bin/env python3
"""BETA12.22 host gates; synthetic GS/GX/VU/IPU input only."""
import argparse,json,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output-json',type=Path);a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='gekko2-beta12-22-') as directory:
 d=Path(directory);base=d/'base.json'
 subprocess.run([sys.executable,'tools/test_beta12_21_host.py','--output-json',str(base)],cwd=root,check=True)
 rows=json.loads(base.read_text())
 names=['test_xgkick_live_tag','test_ipu_soft_reset_predictors','test_gs_clut_banks',
        'test_gs_depth_texture','test_controller_r1364','test_gs_clut','test_gif_texture',
        'test_gx_formats_beta12_22','test_gx_output_psm_r1294']
 built=subprocess.check_output([sys.executable,'tools/build_host_core.py',*names],cwd=root,text=True)
 paths=[Path(line) for line in built.splitlines() if Path(line).is_file()];assert len(paths)==len(names)
 coeff=d/'coefficients'
 subprocess.run(['gcc','-O2','-Iinclude','-Isource','tests/test_ipu_coefficients.c',
                 'source/hw/ipu_mpeg.c','-lm','-o',str(coeff)],cwd=root,check=True)
 for name,path in [*zip(names,paths),('ipu-coefficient-oracle',coeff)]:
  r=subprocess.run([str(path)],cwd=root,capture_output=True,text=True)
  rows.append(dict(test=name,exit_code=r.returncode,output=r.stdout+r.stderr))
  print(('PASS ' if not r.returncode else 'FAIL ')+name,flush=True)
  if r.returncode:print(r.stdout+r.stderr,flush=True)
if a.output_json:
 a.output_json.parent.mkdir(parents=True,exist_ok=True)
 a.output_json.write_text(json.dumps(rows,indent=2)+'\n')
assert len(rows)==37 and len({r['test'] for r in rows})==37
assert all(r['exit_code']==0 for r in rows),'host regression failure'
print('PASS 37 BETA12.22 host suites; no Wii hardware/gameplay claim')
