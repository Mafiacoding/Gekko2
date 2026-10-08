"""Focused linked-PPC verification; mocks platform/GX services, not guest CPUs.
Run with the supplied devkitPPC host libraries and Unicorn in the environment.
"""
from pathlib import Path
import argparse, subprocess, json, concurrent.futures, sys
p=argparse.ArgumentParser();p.add_argument('elf');p.add_argument('--nm',required=True);p.add_argument('--output',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
(root/'outputs/verification').mkdir(parents=True,exist_ok=True)
tests=['verify_ee_native_chain_internal.py','verify_ee_chain_vector_internal.py',
 'verify_ee_fpu_focused_internal.py','verify_ee_traps_internal.py','verify_ee_merge_internal.py',
 'verify_residency_internal.py','verify_word_allocator_internal.py','verify_resident_abi_internal.py',
 'verify_native_chain_abi_internal.py','verify_jit_helper_abi_internal.py',
 'verify_iop_blocks_internal.py','verify_iop_block_abi_internal.py','verify_vu_pipeline_internal.py',
 'verify_wii_system_r1330i.py','verify_ee_idle_scheduler_r1330j.py',
 'verify_gx_fifo_r1330i.py','verify_gx_surface_r1300.py','verify_gx_resident_r1330l.py',
 'verify_gx_resident_fallback_r1330l.py','verify_fastmem_r1331.py','verify_optimization_routes_r1331.py']
def run(name):
 cmd=[sys.executable,str(root/'tools'/name),str(Path(a.elf).resolve()),'--nm',a.nm]
 try:
  result=subprocess.run(cmd,cwd=root,capture_output=True,text=True,timeout=240)
  (out/(name+'.log')).write_text(result.stdout+result.stderr)
  record={'test':name,'returncode':result.returncode}
 except subprocess.TimeoutExpired as error:
  (out/(name+'.log')).write_text((error.stdout or b'').decode(errors='replace')+(error.stderr or b'').decode(errors='replace'))
  record={'test':name,'returncode':124}
 print(name,record['returncode'],flush=True);return record
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:results=list(pool.map(run,tests))
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print(f'{sum(x["returncode"]==0 for x in results)}/{len(results)} focused linked-PPC suites passed',flush=True)
raise SystemExit(any(x['returncode'] for x in results))
