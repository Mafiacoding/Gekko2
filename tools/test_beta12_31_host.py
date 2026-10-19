#!/usr/bin/env python3
"""Previous 78-suite baseline plus new profiling/BIOS tool tests."""
import argparse,json,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output-json',type=Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as temp:
 base=Path(temp)/'base.json';subprocess.run([sys.executable,'tools/test_beta12_30_host.py','--output-json',str(base)],cwd=root,check=True);rows=json.loads(base.read_text())
 exe=Path(temp)/'profile';subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-Iinclude','tests/test_runtime_profile_r1332.c','source/core/runtime_profile.c','-o',str(exe)],cwd=root,check=True)
 work=subprocess.check_output([sys.executable,'tools/build_host_core.py','test_runtime_profile_work'],cwd=root,text=True).strip()
 checks=[('runtime_profile_accounting',[str(exe)]),('runtime_profile_real_work',[work]),('runtime_profile_analysis',[sys.executable,'tests/test_runtime_profile_analysis.py']),('bios_directory_synthetic',[sys.executable,'tests/test_bios_investigation.py']),('wii_limits_host',[sys.executable,'tests/test_wii_limits.py'])]
 for name,cmd in checks:
  run=subprocess.run(cmd,cwd=root,capture_output=True,text=True);rows.append(dict(test=name,exit_code=run.returncode,output=run.stdout+run.stderr));print(name,run.stdout+run.stderr,flush=True)
a.output_json.parent.mkdir(parents=True,exist_ok=True);a.output_json.write_text(json.dumps(rows,indent=2)+'\n')
assert len(rows)==83 and all(x['exit_code']==0 for x in rows)
print('PASS 83 host suites; hardware/GX timing and BIOS tile correction unverified')
