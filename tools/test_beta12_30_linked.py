#!/usr/bin/env python3
"""Reproduce completion PPC gates on both delivered Wii ELFs.
Unicorn models platform/cache services; these are not physical Wii tests.
"""
from pathlib import Path
import argparse,json,subprocess,sys,time
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('elf',type=Path);p.add_argument('--control-elf',type=Path,required=True)
p.add_argument('--nm',required=True);p.add_argument('--output-json',type=Path,required=True)
a=p.parse_args();rows=[]
checks=[(name,name,a.elf) for name in (
 'verify_chain_residency_linked','verify_chain_alu_linked','verify_chain_memory_linked',
 'verify_chain_branch_linked','verify_alpha_modes_linked','verify_gx_formats_linked',
 'verify_ppc_gs_memory','verify_beta12_22_linked','verify_vif_uv_linked','verify_cf_blocks_linked')]
checks.extend([('verify_completion_ee_linked_chain','verify_completion_ee_linked',a.elf),
 ('verify_completion_ee_linked_control','verify_completion_ee_linked',a.control_elf),
 ('verify_gs_vif_accuracy_linked_chain','verify_gs_vif_accuracy_linked',a.elf),
 ('verify_gs_vif_accuracy_linked_control','verify_gs_vif_accuracy_linked',a.control_elf)])
a.output_json.parent.mkdir(parents=True,exist_ok=True)
for name,script,elf in checks:
 start=time.monotonic()
 r=subprocess.run([sys.executable,str(root/'tools'/f'{script}.py'),str(elf.resolve()),'--nm',a.nm],cwd=root,capture_output=True,text=True)
 rows.append(dict(test=name,exit_code=r.returncode,seconds=round(time.monotonic()-start,2),output=r.stdout+r.stderr))
 a.output_json.write_text(json.dumps(rows,indent=2)+'\n')
 print(('PASS ' if r.returncode==0 else 'FAIL ')+name+'\n'+r.stdout+r.stderr,flush=True)
 if r.returncode:raise SystemExit(r.returncode)
assert len(rows)==14
print('PASS 14 linked-PPC suites; no Wii boot, GPU equality or speed claim')
