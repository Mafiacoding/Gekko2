#!/usr/bin/env python3
"""Targeted CPU-status/menu regression; no JIT/GX execution claim."""
import json,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
results=[]
with tempfile.TemporaryDirectory() as temp:
 for name,source in [('launcher','test_frontend_cpu_status'),('options','test_optimization_r1331'),('options1354','test_optimization_r1354')]:
  exe=str(Path(temp)/name)
  subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DGEKKO','-Iinclude','tests/'+source+'.c','source/core/recompiler/optimization.c','-o',exe],cwd=root,check=True)
  result=subprocess.run([exe],cwd=root,text=True,capture_output=True)
  results.append(dict(test=name,exit_code=result.returncode,output=result.stdout+result.stderr))
 result=subprocess.run([sys.executable,'tests/test_runtime_profile_analysis.py'],cwd=root,text=True,capture_output=True)
 results.append(dict(test='profile_analysis',exit_code=result.returncode,output=result.stdout+result.stderr))
print(json.dumps(results,indent=2))
assert all(r['exit_code']==0 for r in results)
