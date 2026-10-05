"""Check actual backend acceptance against vendored primary opcode tables.
Acceptance alone is not an architectural-correctness or native-only claim.
"""
from pathlib import Path
import re,ctypes as C,subprocess,json
root=Path(__file__).resolve().parents[1];out=root/'outputs/verification';out.mkdir(parents=True,exist_ok=True)
text=(root/'docs/reference/pcsx2/pcsx2/R5900OpcodeTables.cpp').read_text()
subprocess.run(['gcc','-O2','-shared','-fPIC','-I'+str(root/'include'),str(root/'source/core/recompiler/ppc_dynarec.c'),'-o',str(out/'libmmi_audit.so')],check=True)
class Context(C.Structure):_fields_=[('code',C.POINTER(C.c_uint32)),('capacity',C.c_size_t),('used',C.c_size_t)]
lib=C.CDLL(str(out/'libmmi_audit.so'));rows=[]
for table,fn in [('MMI',None),('MMI0',8),('MMI1',40),('MMI2',9),('MMI3',41)]:
 names=[x.strip() for x in re.search(r'tbl_'+table+r'\[\d+\]\s*=\s*\{(.*?)\};',text,re.S).group(1).split(',') if x.strip()]
 for pos,name in enumerate(names):
  if name in ['MMI_Unknown','MMI0','MMI1','MMI2','MMI3']:continue
  for mode in (range(5) if name=='PMFHL' else [pos if fn is not None else 0]):
   opcode=(28<<26)|(1<<21)|(2<<16)|(3<<11)|(mode<<6)|(pos if fn is None else fn)
   code=(C.c_uint32*129)();ctx=Context(code,129,0)
   accepted=lib.ppc_dynarec_translate_one(C.byref(ctx),opcode)==0
   assert ctx.used<=128
   rows.append({'name':name,'mode':mode,'accepted':accepted,'words':ctx.used})
(out/'mmi_coverage_internal.json').write_text(json.dumps(rows,indent=2))
print('Legal encodings',len(rows),'accepted',sum(x['accepted'] for x in rows),'declined',[x for x in rows if not x['accepted']])
assert all(x['accepted'] for x in rows)
