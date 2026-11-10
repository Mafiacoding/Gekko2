#!/usr/bin/env python3
"""BETA12.23 host gates; synthetic GS/GX/VU/IPU input only."""
import argparse,json,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output-json',type=Path);a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='gekko2-beta12-23-') as directory:
 d=Path(directory);base=d/'base.json'
 subprocess.run([sys.executable,'tools/test_beta12_22_host.py','--output-json',str(base)],cwd=root,check=True)
 rows=json.loads(base.read_text())
 names=['test_vu_integer_memory_masks','test_vif_micro_barriers','test_gs_uv_fraction',
        'test_vu_micro','test_vu_q_pipeline_internal','test_vif','test_vif_payload_r1379',
        'test_gif_packed_tex_clamp','test_gs_display_scaling','test_xfb_aspect']
 built=subprocess.check_output([sys.executable,'tools/build_host_core.py',*names],cwd=root,text=True)
 paths=[Path(line) for line in built.splitlines() if Path(line).is_file()];assert len(paths)==len(names)
 for name,path in zip(names,paths):
  r=subprocess.run([str(path)],cwd=root,capture_output=True,text=True)
  rows.append(dict(test=name,exit_code=r.returncode,output=r.stdout+r.stderr))
  print(('PASS ' if not r.returncode else 'FAIL ')+name,flush=True)
  if r.returncode:print(r.stdout+r.stderr,flush=True)
if a.output_json:
 a.output_json.parent.mkdir(parents=True,exist_ok=True)
 a.output_json.write_text(json.dumps(rows,indent=2)+'\n')
assert len(rows)==47 and len({r['test'] for r in rows})==47
assert all(r['exit_code']==0 for r in rows),'host regression failure'
print('PASS 47 BETA12.23 host suites; no Wii hardware/gameplay claim')
