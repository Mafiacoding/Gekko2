#!/usr/bin/env python3
"""Inventory tracked runtime sources and real EE compiler admission.

This measures acceptance, not opcode correctness or hardware conformance.
Comment markers are review candidates, never automatically confirmed bugs.
No BIOS/disc/card/profile inputs are read.
"""
from pathlib import Path
import argparse,ctypes as C,hashlib,json,re,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
def uncomment(s):
 return re.sub(r'//[^\n]*','',re.sub(r'/\*.*?\*/','',s,flags=re.S))
def body(s,signature):
 s=uncomment(s);start=s.index('{',s.index(signature));depth=1;i=start+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
def forms():
 reference=(ROOT/'docs/reference/pcsx2/pcsx2/R5900OpcodeTables.cpp').read_text()
 def table(name):
  text=re.search(r'tbl_'+name+r'\[\d+\]\s*=\s*\{(.*?)\};',reference,re.S).group(1)
  return [n.strip() for n in uncomment(text).split(',') if n.strip()]
 groups={n:table(n) for n in ['Standard','Special','RegImm','MMI','MMI0','MMI1','MMI2','MMI3','COP0','COP0_BC0','COP0_C0','COP1','COP1_BC1','COP1_S','COP1_W']}
 # Canonical legal representatives, not a promise about ignored/reserved bits.
 routes={'SPECIAL':('Special',0x00000000,0),'REGIMM':('RegImm',1<<26,16),
  'MMI':('MMI',28<<26,0),'MMI0':('MMI0',(28<<26)|8,6),'MMI1':('MMI1',(28<<26)|40,6),
  'MMI2':('MMI2',(28<<26)|9,6),'MMI3':('MMI3',(28<<26)|41,6),
  'COP0':('COP0',16<<26,21),'COP0_BC0':('COP0_BC0',(16<<26)|(8<<21),16),
  'COP0_C0':('COP0_C0',(16<<26)|(16<<21),0),
  'COP1':('COP1',17<<26,21),'COP1_BC1':('COP1_BC1',(17<<26)|(8<<21),16),
  'COP1_S':('COP1_S',(17<<26)|(16<<21),0),'COP1_W':('COP1_W',(17<<26)|(20<<21),0)}
 rows=[]
 def walk(tab,base,shift):
  for index,name in enumerate(groups[tab]):
   if 'Unknown' in name or name=='COP2':continue
   word=base|(index<<shift)
   if name in routes:
    child,bits,selector=routes[name];walk(child,bits,selector);continue
   # Use nonzero operands while keeping selector/reserved fields canonical.
   op=word>>26
   if tab=='Standard':word|=(1<<21)|(2<<16)|16
   elif tab=='Special':
    word|=(1<<21)|(2<<16)|(3<<11)
    if name in ('SLL','SRL','SRA','DSLL','DSRL','DSRA','DSLL32','DSRL32','DSRA32','MFHI','MFLO','MFSA'):word&=~(31<<21)
    if name in ('JR','JALR','MFHI','MFLO','MFSA','MTHI','MTLO','MTSA'):word&=~(31<<16)
    if name in ('JR','MTHI','MTLO','MTSA'):word&=~(31<<11)
    if name in ('SYNC','SYSCALL','BREAK'):word=index
   elif tab=='RegImm':word|=(1<<21)|16
   elif tab.startswith('MMI'):word|=(1<<21)|(2<<16)|(3<<11)
   elif tab=='COP0':word|=(2<<16)|(3<<11)
   elif tab=='COP0_BC0':word|=16
   elif tab=='COP1':word|=(2<<16)|(3<<11)
   elif tab=='COP1_BC1':word|=16
   elif tab in ('COP1_S','COP1_W'):word|=(2<<16)|(3<<11)|((0 if name in ('C_F','C_EQ','C_LT','C_LE') else 4)<<6)
   for mode in range(5) if name=='PMFHL' else [0]:
    iw=word|(mode<<6);rows.append({'family':tab,'name':name+('.'+str(mode) if name=='PMFHL' else ''),'word':iw})
 walk('Standard',0,26)
 # COP2 has a separate R5900/VU decode, not the scalar opcode tables above.
 for rs,name in [(0,'MFC2'),(1,'QMFC2'),(2,'CFC2'),(4,'MTC2'),(5,'QMTC2'),(6,'CTC2')]:
  rows.append({'family':'COP2','name':name,'word':(18<<26)|(rs<<21)|(2<<16)|(3<<11)})
 for rt,name in enumerate(['BC2F','BC2T','BC2FL','BC2TL']):rows.append({'family':'COP2','name':name,'word':(18<<26)|(8<<21)|(rt<<16)|16})
 rows.append({'family':'COP0','name':'MTC0.EntryHi','word':(16<<26)|(4<<21)|(2<<16)|(10<<11)})
 return rows
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 front=(ROOT/'source/core/recompiler/ee_jit.c').read_text()
 with tempfile.TemporaryDirectory(prefix='gekko2-completion-') as tmp:
  src=Path(tmp)/'probe.c';lib=Path(tmp)/'probe.so'
  src.write_text('#include "core/recompiler/ppc_dynarec.c"\n'
   +'static int front(uint32_t instr)'+body(front,'static int ee_jit_opcode_supported(')+'\n'+r'''
unsigned probe(uint32_t word,unsigned *words,unsigned *calls){
 ppc_codegen_ctx_t c;*words=*calls=0;if(ppc_dynarec_init(&c,4))return 128;
 unsigned bits=front(word)?1:0;int r=ppc_dynarec_translate_one(&c,word);
 if(!r){bits|=2;*words=c.used_words;for(unsigned i=0;i<c.used_words;i++)if(c.code[i]==0x4e800421u)(*calls)++;}
 ppc_dynarec_free(&c);
 if(ee_jit_block_candidate(word))bits|=4;if(ee_jit_block_terminal(word))bits|=8;
 uint32_t pair[]={word,0};if(!ee_chain_accesses(pair,2,0))bits|=16;
 ppc_codegen_ctx_t block;if(ppc_dynarec_init(&block,8))return 128;
 if(!ppc_dynarec_translate_ee_resident_delay_block(&block,0x200000,pair,2,0x1000,0x2000,0x3000,0x4000,1024))bits|=32;
 ppc_dynarec_free(&block);return bits;
}''')
  subprocess.run(['gcc','-shared','-fPIC','-O2','-Iinclude','-Isource',str(src),'-o',str(lib)],cwd=ROOT,check=True)
  backend=C.CDLL(str(lib));backend.probe.argtypes=[C.c_uint32,C.POINTER(C.c_uint),C.POINTER(C.c_uint)]
  rows=forms()
  for row in rows:
   n=C.c_uint();calls=C.c_uint();bits=backend.probe(row['word'],C.byref(n),C.byref(calls));assert not(bits&128)
   row.update(word=f'{row["word"]:08x}',single_front=bool(bits&1),emitter=bool(bits&2),block_policy=bool(bits&4),terminal=bool(bits&8),chain_policy=bool(bits&16),block_emitter=bool(bits&32),emitted_words=n.value,opaque_call_sites=calls.value)
 files=subprocess.check_output(['git','ls-files','source','include'],cwd=ROOT,text=True).splitlines();modules=[];markers=[]
 marker=re.compile(r'\b(TODO|FIXME|stub|placeholder|unimplemented|unsupported|simplif\w*|approxim\w*)\b|not (?:yet )?implemented',re.I)
 for name in files:
  path=ROOT/name
  if path.suffix not in ('.c','.h','.inc','.s'):continue
  data=path.read_bytes();modules.append({'path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
  for n,line in enumerate(data.decode(errors='replace').splitlines(),1):
   if marker.search(line):markers.append({'path':name,'line':n,'text':line.strip()[:200],'classification':'review_candidate_not_a_confirmed_bug'})
 result={'schema':1,'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
  'scope':'All tracked runtime source and include files; scalar EE canonical representatives plus COP2 transfers/branches. VU micro and COP2 macro completeness need their separate audits.',
  'limitations':['Compiler acceptance is not executed PPC correctness.','Memory admission needs live translation/alias guards.','Opaque-call count does not classify call semantics.','Canonical representatives do not exhaust operand aliases or reserved-bit variants.','Comment markers may describe historical fixes, legitimate fallbacks or unsupported reserved encodings.'],
  'runtime_sources':modules,'ee_forms':rows,'review_markers':markers}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n')
 print('Runtime sources:',len(modules),'EE forms:',len(rows),'comment review candidates:',len(markers))
 gaps=[r for r in rows if not(r['single_front'] and r['emitter'])]
 print('Scalar/single-op gaps:',', '.join(r['name'] for r in gaps))
 print('Emitter-backed but single-gate-declined:',', '.join(r['name'] for r in rows if r['emitter'] and not r['single_front']))
if __name__=='__main__':main()
