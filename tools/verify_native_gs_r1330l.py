"""Compile current portable sources once, then run focused GS regressions.
Self-included source is omitted explicitly, as in tests/run_test.sh.
"""
from pathlib import Path
import argparse, re, subprocess, json, concurrent.futures
p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
sources=sorted(s for s in (root/'source').rglob('*.c') if s.name not in ('main.c','ppc_dynarec.c'))
def build_source(src):
 obj=out/(str(src.relative_to(root)).replace('/','_')+'.o')
 result=subprocess.run(['gcc','-O2','-w','-I'+str(root/'include'),'-I'+str(root/'source'),'-c',str(src),'-o',str(obj)],capture_output=True,text=True)
 if result.returncode:raise RuntimeError(result.stderr)
 return src,obj
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:objects=dict(pool.map(build_source,sources))
selected=sorted(set((root/'tests').glob('test_gs_*.c')) | set((root/'tests').glob('test_gif_*.c')) | set((root/'tests').glob('test_gx_*.c')) | {root/'tests'/n for n in [
 'test_gif.c','test_boot_gx_policy_r1291.c','test_checkpoint_sprite_r1297.c',
 'test_display_content_probe.c','test_dma_gif_demo.c','test_ee_display_clock.c',
 'test_sprite_cache_r1295.c','test_sprite_subpixel_r1297.c','test_gouraud_counters_r1330k.c']})
results=[]
for src in selected:
 excluded=set(re.findall(r'#include "((?:core|hw)/[a-zA-Z0-9_/]+\.c)"',src.read_text()))
 binary=out/src.stem;log=out/(src.stem+'.log')
 command=['gcc','-O2','-w','-I'+str(root/'include'),'-I'+str(root/'source'),str(src)]+[str(obj) for source,obj in objects.items() if str(source.relative_to(root/'source')) not in excluded]+['-lm','-o',str(binary)]
 build=subprocess.run(command,capture_output=True,text=True)
 run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=60) if build.returncode==0 else build
 log.write_text(build.stdout+build.stderr+run.stdout+run.stderr)
 results.append({'test':src.stem,'returncode':run.returncode})
 print(src.stem,run.returncode,flush=True)
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print(f'{sum(r["returncode"]==0 for r in results)}/{len(results)} native GS regressions passed',flush=True)
raise SystemExit(any(r['returncode'] for r in results))
