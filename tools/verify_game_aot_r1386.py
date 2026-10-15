"""Linked game AOT versus interpreter, bounded instructions and full RAM state.

Private executable/profile words are supplied at runtime, never bundled here.
This is a differential CPU check, not a whole-game or physical Wii test.
"""
import argparse,json,sys,importlib.util,hashlib
from pathlib import Path
p=argparse.ArgumentParser(add_help=False);p.add_argument('--profile',required=True)
options,remaining=p.parse_known_args();sys.argv=[sys.argv[0],*remaining]
fixture=Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text()
exec(compile(fixture,'ee_ppc_fixture_internal.py','exec'))
spec=importlib.util.spec_from_file_location('gen',root/'tools/ee_aot_generate.py')
gen=importlib.util.module_from_spec(spec);spec.loader.exec_module(gen)
u.mem_map(ram+0x400000,0x1c00000);zero=bytes(0x2000000)
mask=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask,4),'big')
records=json.loads(Path(options.profile).read_text())['blocks'];selected=[]
for r in records:
 if r.get('provenance')!='static executable decode':continue
 pc=gen.number(r['pc']);words=gen.supported_prefix(pc,r['words'],32)
 # VCALLMS needs the game microprogram/IO state; that has its own VU gate.
 if not words or any(w>>26==0x12 and w>>21&16 and w&63 in (56,57) for w in words[:4]):continue
 selected.append((pc,words))
 if len(selected)==32:break
assert selected
checks=hits=0
for pc,words in selected:
 for budget in sorted({1,min(2,len(words)),min(4,len(words))}):
  snapshots=[]
  for enabled in (False,True):
   setup();u.mem_write(ram,zero);word(state+off['ram_size'],0x2000000)
   for name in ['sif_init','vif0_init','vif1_init','gs_init','vu1_init']:
    if name in syms:call(name)
   word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
   page=pc&~8191;physical=page&0x1fffffff
   u.mem_write(state+off['tlb'],struct.pack('>4I',0,page|7,((physical>>12)<<6)|6,(((physical>>12)+1)<<6)|6))
   u.mem_write(ram+(pc&0x1fffffff),struct.pack('<'+str(len(words))+'I',*words))
   for r in range(1,32):u.mem_write(state+r*16,struct.pack('>QQ',0x80300000+r*64,0x123456789abcdef0+r))
   word(mask,((initial|1)&~(8|2|(1<<24)))|(8 if enabled else 0))
   u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8))
   high=call('ee_aot_retired_count');before=(high<<32)|u.reg_read(UC_PPC_REG_4)
   assert call('ee_core_step_n',budget)==budget,(hex(pc),budget,enabled)
   high=call('ee_aot_retired_count');after=(high<<32)|u.reg_read(UC_PPC_REG_4)
   if enabled and after>before:hits+=1
   call('ee_timers_snapshot',0x817f0000)
   snapshots.append(bytes(u.mem_read(state,size))+bytes(u.mem_read(0x817f0000,64))+bytes(u.mem_read(syms['g_ee_display_ticks'],8))+bytes(u.mem_read(syms['g_ee_display_phase'],4))+bytes(u.mem_read(syms['g_ee_irq'],8))+hashlib.sha256(u.mem_read(ram,0x2000000)).digest())
  assert snapshots[0]==snapshots[1],(hex(pc),budget,'game AOT/interpreter mismatch')
  checks+=1
assert checks>=64 and hits>0
print(json.dumps({'game_full_state_comparisons':checks,'aot_exercised':hits,'all_passed':True,'physical_wii_verified':False}))
