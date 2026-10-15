"""Actual linked AOT windows versus precise AOT; private profile supplies code."""
import argparse,sys,json,importlib.util
from pathlib import Path
extra=argparse.ArgumentParser(add_help=False);extra.add_argument('--profile',required=True);extra.add_argument('--bios',required=True)
opts,remaining=extra.parse_known_args();sys.argv=[sys.argv[0],*remaining]
text=Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text().replace("'gpr_generation']","'gpr_generation','bios']")
exec(compile(text,'fixture','exec'))
spec=importlib.util.spec_from_file_location('gen',root/'tools/ee_aot_generate.py');gen=importlib.util.module_from_spec(spec);spec.loader.exec_module(gen)
records=json.loads(Path(opts.profile).read_text())['blocks'];selected=[]
for record in sorted(records,key=lambda r:-int(r.get('selection_weight',r.get('hits',0)))):
 pc=gen.number(record['pc']);words=gen.supported_prefix(pc,record['words'],32)
 if words:selected.append((pc,words))
 if len(selected)==call('ee_aot_block_count'):break
candidates=[]
for pc,words in selected:
 n=0
 while n<len(words) and gen.pure_window_candidate(words[n]):n+=1
 if n>=2 and (pc&0x1fffffff)<0x400000:candidates.append((pc,words,n))
assert candidates
mask=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask,4),'big');checks=0;costs=[];windows=0
for pc,words,prefix in sorted(candidates,key=lambda r:-r[2])[:12]:
 for budget in sorted({1,2,prefix-1,prefix}):
  states=[];counts=[]
  for fast in (False,True):
   setup();word(mask,(initial|1|8|(1<<20))&~(1<<24) | ((1<<24) if fast else 0))
   for name in ['sif_init','vif0_init','vif1_init','vu1_init']:
    if name in syms:call(name)
   word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
   page=pc&~8191;physical=page&0x1fffffff
   u.mem_write(state+off['tlb'],struct.pack('>4I',0,page|7,((physical>>12)<<6)|2,(((physical>>12)+1)<<6)|2))
   u.mem_write(ram+(pc&0x1fffffff),struct.pack('<'+str(len(words))+'I',*words))
   for r in range(1,32):u.mem_write(state+r*16,struct.pack('>QQ',(0x80300000+r*19)&0xffffffff,0xfedcba9876543210+r))
   u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8));call('ee_timers_tick')
   before=call('ee_core_native_window_stat',0);before=(before<<32)|u.reg_read(UC_PPC_REG_4)
   total[0]=0;active[0]=True;got=call('ee_aot_try_execute_fetched',state,budget,words[0]);active[0]=False
   assert got==budget,(hex(pc),budget,fast,got)
   counts.append(total[0]);call('ee_timers_snapshot',0x817e0000)
   states.append(bytes(u.mem_read(state,size))+bytes(u.mem_read(0x817e0000,64))+bytes(u.mem_read(syms['g_ee_display_ticks'],8))+bytes(u.mem_read(syms['g_ee_irq'],8)))
   after=call('ee_core_native_window_stat',0);after=(after<<32)|u.reg_read(UC_PPC_REG_4)
   if fast:windows+=after-before
  assert states[0]==states[1],(hex(pc),budget,next(i for i,(x,y) in enumerate(zip(states[0],states[1])) if x!=y))
  checks+=1
  if budget==prefix:costs.append({'guest_instructions':budget,'precise_PPC_instructions':counts[0],'window_PPC_instructions':counts[1]})
assert windows>0
print(json.dumps({'full_state_comparisons':checks,'windows_exercised':windows,'costs':costs,'physical_wii_verified':False,'scope':'Real linked AOT/retirement instructions; mocked platform, no game FPS claim.'},indent=2))
