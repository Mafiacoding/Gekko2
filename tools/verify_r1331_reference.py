"""Compare representative scalar/reference guest-state signatures to Options."""
from pathlib import Path
import argparse,subprocess,json,concurrent.futures,sys,re
p=argparse.ArgumentParser();p.add_argument('elf');p.add_argument('--nm',required=True);p.add_argument('--output',required=True);p.add_argument('--options-results',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
tests=['verify_ee_native_chain_internal.py','verify_ee_chain_vector_internal.py','verify_ee_fpu_focused_internal.py','verify_wii_system_r1330i.py','verify_ee_idle_scheduler_r1330j.py','verify_mapped_r1331.py']
def run(name):
 r=subprocess.run([sys.executable,str(root/'tools'/name),str(Path(a.elf).resolve()),'--nm',a.nm],cwd=root,capture_output=True,text=True,timeout=180)
 (out/(name+'.log')).write_text(r.stdout+r.stderr)
 previous=Path(a.options_results)/(name+'.log')
 parity=[]
 if previous.exists():
  expected=dict(re.findall(r'^([A-Z_]+SIGNATURE) ([0-9a-f]+)$',previous.read_text(),re.M))
  actual=dict(re.findall(r'^([A-Z_]+SIGNATURE) ([0-9a-f]+)$',r.stdout,re.M))
  parity=[{'signature':k,'matches':actual.get(k)==v} for k,v in expected.items()]
 result={'test':name,'returncode':r.returncode,'signature_parity':parity}
 print(json.dumps(result),flush=True);return result
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(run,tests))
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(any(r['returncode'] or any(not v['matches'] for v in r['signature_parity']) for r in results))
