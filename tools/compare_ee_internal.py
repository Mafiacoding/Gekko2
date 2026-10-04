"""Require paired architectural signatures, not merely successful executions."""
import argparse,json,re
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('jit_log',type=Path);p.add_argument('interpreter_log',type=Path)
p.add_argument('--gs-jit',type=Path);p.add_argument('--gs-interpreter',type=Path)
a=p.parse_args()
def signatures(path):
 return dict(re.findall(r'^(EE_[A-Z0-9_]*SIGNATURE) ([0-9a-f]{64})$',path.read_text(),re.M))
jit=signatures(a.jit_log);scalar=signatures(a.interpreter_log)
required={'EE_DELAY_SIGNATURE','EE_VECTOR_SIGNATURE','EE_CHAIN_SIGNATURE','EE_FPU_ARITHMETIC_SIGNATURE','EE_EXTENDED_LINK_SIGNATURE'}
assert required<=jit.keys() and required<=scalar.keys(),('missing required final signature',required-jit.keys(),required-scalar.keys())
assert jit.keys()==scalar.keys(),('different coverage',jit.keys(),scalar.keys())
assert jit==scalar,('architectural state mismatch',[k for k in jit if jit[k]!=scalar[k]])
print(f'PASS {len(jit)} paired EE architectural signatures')
if a.gs_jit or a.gs_interpreter:
 assert a.gs_jit and a.gs_interpreter
 def gs(path):
  data=json.loads(next(l.split(' ',1)[1] for l in path.read_text().splitlines() if l.startswith('GS_PATHS_R1285 ')))
  return {k:v['vram_sha256'] for k,v in data['results'].items()}
 g=gs(a.gs_jit);s=gs(a.gs_interpreter)
 assert len(g)==66 and g==s,('GS VRAM mismatch',[k for k in g if g.get(k)!=s.get(k)])
 print('PASS 66 paired complete VRAM signatures')
