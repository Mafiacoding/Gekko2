from pathlib import Path
import subprocess,re,concurrent.futures,json,time
root=Path(__file__).resolve().parents[1];out=root/'outputs/verification';out.mkdir(parents=True,exist_ok=True)
sources=[p for p in (root/'source').rglob('*.c') if p.name not in ['main.c','ppc_dynarec.c']]
objects={p:out/(str(p.relative_to(root)).replace('/','_')+'.o') for p in sources}
flags=['gcc','-O2','-w','-I'+str(root/'include'),'-I'+str(root/'source')]
def obj(p):
 r=subprocess.run(flags+['-c',str(p),'-o',str(objects[p])],capture_output=True,text=True)
 if r.returncode:raise RuntimeError(r.stderr)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:list(ex.map(obj,sources))
def test(p):
 incs=set(re.findall(r'#include "((?:core|hw)/[a-zA-Z0-9_/]+\.c)"',p.read_text()))
 args=[str(o) for s,o in objects.items() if str(s.relative_to(root/'source')) not in incs]
 exe=out/p.stem;log=out/(p.stem+'.log')
 r=subprocess.run(flags+[str(p)]+args+['-o',str(exe),'-lm'],capture_output=True,text=True)
 if r.returncode:log.write_text(r.stdout+r.stderr);return {'name':p.stem,'status':'compile_fail'}
 try:r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=90)
 except subprocess.TimeoutExpired:return {'name':p.stem,'status':'timeout'}
 log.write_text(r.stdout+r.stderr)
 return {'name':p.stem,'status':'pass' if r.returncode==0 else 'fail','exit':r.returncode,'fail_lines':[l for l in r.stdout.splitlines() if l.startswith('FAIL:')]}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:
 results=[]
 for r in ex.map(test,sorted((root/'tests').glob('test_*.c'))):
  results.append(r);print(r['name'],r['status'],flush=True)
(out/'suite_results.json').write_text(json.dumps(results,indent=2))
print('SUMMARY',len(results),'PASS',sum(r['status']=='pass' for r in results),flush=True)
