#!/usr/bin/env python3
"""Run independent linked PPC gates concurrently; each process owns its model."""
import argparse,concurrent.futures,json,subprocess,sys,time
from pathlib import Path
root=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser(description=__doc__)
p.add_argument('elf',type=Path);p.add_argument('--control-elf',type=Path,required=True);p.add_argument('--nm',required=True);p.add_argument('--jobs',type=int,default=2);p.add_argument('--output-json',type=Path,required=True);a=p.parse_args()
checks=[(x,x,a.elf) for x in ('verify_chain_residency_linked','verify_chain_alu_linked','verify_chain_memory_linked','verify_chain_branch_linked','verify_alpha_modes_linked','verify_gx_formats_linked','verify_ppc_gs_memory','verify_beta12_22_linked','verify_vif_uv_linked','verify_cf_blocks_linked')]
for label,elf in (('chain',a.elf),('control',a.control_elf)):
 for script in ('verify_completion_ee_linked','verify_gs_vif_accuracy_linked','verify_profile_r1332'):checks.append((script+'_'+label,script,elf))
def run(item):
 name,script,elf=item;begin=time.monotonic();r=subprocess.run([sys.executable,str(root/'tools'/f'{script}.py'),str(elf.resolve()),'--nm',a.nm],cwd=root,capture_output=True,text=True)
 return dict(test=name,exit_code=r.returncode,seconds=round(time.monotonic()-begin,2),output=r.stdout+r.stderr)
rows=[];a.output_json.parent.mkdir(parents=True,exist_ok=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=max(1,min(a.jobs,4))) as pool:
 for f in concurrent.futures.as_completed([pool.submit(run,x) for x in checks]):
  row=f.result();rows.append(row);a.output_json.write_text(json.dumps(rows,indent=2)+'\n');print(row['test'],row['exit_code'],row['output'],flush=True)
assert len(rows)==16 and all(x['exit_code']==0 for x in rows)
print('PASS 16 actual-ELF PPC suites; no physical Wii run or speed claim')
