"""Execute linked AOT and interpreter on modeled PPC; never claim physical Wii FPS."""
import argparse
import json
from pathlib import Path
import struct
import sys

extra=argparse.ArgumentParser(add_help=False)
extra.add_argument('--profile',required=True)
extra.add_argument('--bios',required=True)
options, remaining=extra.parse_known_args()
sys.argv=[sys.argv[0],*remaining]
fixture=Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text()
fixture=fixture.replace("'gpr_generation']","'gpr_generation','bios','cop2_ctrl']")
exec(compile(fixture,'ee_ppc_fixture_internal.py','exec'))
import importlib.util
spec=importlib.util.spec_from_file_location('aot',root/'tools/ee_aot_generate.py')
aot=importlib.util.module_from_spec(spec);spec.loader.exec_module(aot)
rom=Path(options.bios).read_bytes();rom_base=0x92000000
u.mem_map(rom_base,0x400000);u.mem_write(rom_base,rom)
bios_descriptor=0x817e0000
word(bios_descriptor,rom_base);word(bios_descriptor+4,len(rom))
initial_mask=int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')
records=json.loads(Path(options.profile).read_text())['blocks']
records=sorted(records,key=lambda b:-int(b.get('selection_weight',b.get('hits',0))))
profile_blocks=[]
for record in records:
 if aot.translate(aot.number(record['words'][0]),aot.number(record['pc'])) is not None:
  profile_blocks.append(record)
  if len(profile_blocks)==call('ee_aot_block_count'):break
records=profile_blocks
selected=[]
for record in records:
 pc=aot.number(record['pc']);words=aot.supported_prefix(pc,record['words'],32)
 if words and (pc&0xc0000000)==0x80000000 and (pc&0x1fffffff)>=0x1fc00000:
  selected.append((pc,words))
 if len(selected)==48:break
assert selected
comparisons=hits=0
for pc,program in selected:
 for budget in sorted(set([1,min(2,len(program)),min(4,len(program))])):
  snapshots=[];retirements=[]
  for use_aot in (False,True):
   setup();word(state+off['bios'],bios_descriptor)
   word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
   for reg in range(1,32):u.mem_write(state+reg*16,struct.pack('>QQ',0x80300000,0x123456789abcdef0))
   u.mem_write(ram+0x300000,bytes(range(128)))
   word(syms['gekko2_optimization_mask'],(initial_mask|1|8) if use_aot else (initial_mask|1)&~8)
   u.mem_write(syms['g_ee_display_ticks'],struct.pack('>Q',1000));word(syms['g_ee_display_phase'],1000);u.mem_write(syms['g_ee_irq'],bytes(8))
   high=call('ee_aot_retired_count');before=(high<<32)|u.reg_read(UC_PPC_REG_4)
   assert call('ee_core_step_n',budget)==budget,(hex(pc),budget,use_aot)
   high=call('ee_aot_retired_count');after=(high<<32)|u.reg_read(UC_PPC_REG_4)
   if use_aot and after>before:hits+=1
   snapshot=bytes(u.mem_read(state,size))+bytes(u.mem_read(ram+0x300000,128))
   call('ee_timers_snapshot',0x817f0000);snapshot+=bytes(u.mem_read(0x817f0000,64))
   snapshot+=bytes(u.mem_read(syms['g_ee_display_ticks'],8))+bytes(u.mem_read(syms['g_ee_display_phase'],4))+bytes(u.mem_read(syms['g_ee_irq'],8))
   snapshots.append(snapshot)
  assert snapshots[0]==snapshots[1],(hex(pc),budget,'AOT/interpreter PPC state mismatch')
  comparisons+=1
assert hits
print(json.dumps({'linked_ppc_comparisons':comparisons,'aot_exercised_comparisons':hits,'all_passed':True,'physical_wii_verified':False}))

# Derive actual target layouts; pointers and uint64 alignment are PPC EABI.
import tempfile
def layout(header,type_name,fields,prefix=''):
 with tempfile.TemporaryDirectory() as d:
  source=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
  source.write_text('#include <stddef.h>\n#include "'+header+'"\n'+prefix+'\nconst unsigned layout[]={sizeof('+type_name+'),'+','.join('offsetof('+type_name+','+f+')' for f in fields)+'};\n')
  subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(source),'-o',str(obj)],check=True)
  subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
  size,*offsets=struct.unpack('>'+str(len(fields)+1)+'I',raw.read_bytes())
  return size,dict(zip(fields,offsets))
isize,ioff=layout('core/iop/iop_core.h','iop_state_t',['pc','next_pc','ram','ram_size','bios','sched_ticks'])
iostate=call('iop_core_get_state');ioram=0x90600000;u.mem_map(ioram,0x200000)
spec=importlib.util.spec_from_file_location('iop_aot',root/'tools/iop_aot_generate.py');iop_aot=importlib.util.module_from_spec(spec);spec.loader.exec_module(iop_aot)
iorecords=sorted(json.loads(Path(options.profile).read_text())['iop_blocks'],key=lambda r:-int(r.get('selection_weight',r.get('hits',0))))
iorecords=[r for r in iorecords if iop_aot.supported(aot.number(r['words'][0]))][:256]
io_comparisons=io_hits=0
for record in [r for r in iorecords if (aot.number(r['pc'])&0x1fffffff)>=0x100000][:32]:
 pc=aot.number(record['pc']);snapshots=[]
 for enabled in (False,True):
  setup()
  for name in ['iop_intc_init','iop_timers_init','iop_hle_thread_init','iop_asyncio_init']:call(name)
  u.mem_write(iostate,bytes(isize));word(iostate+ioff['ram'],ioram);word(iostate+ioff['ram_size'],0x200000);word(iostate+ioff['bios'],bios_descriptor)
  word(iostate+ioff['pc'],pc);word(iostate+ioff['next_pc'],pc+4);u.mem_write(iostate+ioff['sched_ticks'],struct.pack('>Q',1000))
  for reg in range(1,32):word(iostate+4*reg,0x80180000)
  u.mem_write(ioram+0x180000,bytes(range(128)))
  if (pc&0x1fffffff)<0x200000:u.mem_write(ioram+(pc&0x1fffffff),struct.pack('<I',aot.number(record['words'][0])))
  word(syms['gekko2_optimization_mask'],(initial_mask|2) if enabled else initial_mask&~2)
  high=call('iop_aot_executed_count');before=(high<<32)|u.reg_read(UC_PPC_REG_4)
  assert call('iop_core_step_one_native')==1
  high=call('iop_aot_executed_count');after=(high<<32)|u.reg_read(UC_PPC_REG_4)
  if enabled and after>before:io_hits+=1
  snapshots.append(bytes(u.mem_read(iostate,isize))+bytes(u.mem_read(ioram+0x180000,128)))
 assert snapshots[0]==snapshots[1],('IOP PPC AOT mismatch',hex(pc));io_comparisons+=1
assert io_hits
print(json.dumps({'iop_linked_ppc_comparisons':io_comparisons,'iop_aot_exercised':io_hits,'all_passed':True}))

definition='typedef struct {uint32_t vf[32][4],vi[32],acc[4];uint8_t mem[16384],micro[16384];uint32_t pc,bd,bt,ed;uint64_t retired,unknown;vu_pipeline_t pipeline;} context;'
vsize,voff=layout('core/hw/vu.h','context',['vf','vi','acc','mem','micro','pc','bd','bt','ed','retired','unknown','pipeline'],definition)
vctx=0x93000000;u.mem_map(vctx,0x10000)
def call_many(name,*args):
 for i,value in enumerate(args[8:]):word(0x81780008+i*4,value)
 return call(name,*args[:8])
upper_words=[];lower_words=[]
for unit in (0,1):
 image=Path(options.profile+f'.vu{unit}.bin').read_bytes()
 for low,high in struct.iter_unpack('<II',image):
  if high&0x01ffffff and (high&0x01ffffff,low) not in upper_words:upper_words.append((high&0x01ffffff,low))
vu_comparisons=vu_hits=0
for upper,lower in upper_words[:24]:
 for unit in (0,1):
  snapshots=[]
  for enabled in (False,True):
   setup();call('vu1_init');call('vif_init');u.mem_write(vctx,bytes(vsize))
   for reg in range(1,32):
    word(vctx+voff['vi']+reg*4,reg)
    for lane in range(4):word(vctx+voff['vf']+reg*16+lane*4,0x3f800000)
   word(vctx+voff['vf']+12,0x3f800000)
   for lane in range(4):word(vctx+voff['acc']+lane*4,0x3f800000)
   word(vctx+voff['vi']+21*4,0x3f800000);word(vctx+voff['vi']+22*4,0x40000000)
   u.mem_write(vctx+voff['micro'],struct.pack('<II',lower,upper))
   u.mem_write(vctx+voff['mem'],bytes(range(128)))
   word(syms['gekko2_optimization_mask'],(initial_mask|4) if enabled else initial_mask&~4)
   high=call('vu_aot_executed_count',0);before=(high<<32)|u.reg_read(UC_PPC_REG_4)
   mask=16383 if unit else 4095
   args=[vctx+voff[f] for f in ['vf','vi','acc','mem']]+[mask,vctx+voff['micro'],mask]+[vctx+voff[f] for f in ['pc','bd','bt','ed','retired','unknown','pipeline']]
   call_many('vu_micro_step_pipeline',*args)
   high=call('vu_aot_executed_count',0);after=(high<<32)|u.reg_read(UC_PPC_REG_4)
   if enabled and after>before:vu_hits+=1
   snapshots.append(bytes(u.mem_read(vctx,vsize)))
  assert snapshots[0]==snapshots[1],('VU PPC AOT mismatch',hex(upper),hex(lower),unit);vu_comparisons+=1
assert vu_hits
print(json.dumps({'vu_linked_ppc_comparisons':vu_comparisons,'vu_aot_exercised':vu_hits,'all_passed':True,'physical_wii_verified':False}))

# Independent BC2 oracles execute linked target code, beyond interpreter parity.
pc=0x8020f000;bc2_cases=0
for condition in (0,1):
 for kind in range(4):
  setup();word(state+off['bios'],bios_descriptor)
  word(state+off['pc'],pc);word(state+off['next_pc'],pc+4)
  word(state+off['cop2_ctrl']+29*4,condition<<8)
  u.mem_write(ram+(pc&0x1fffffff),struct.pack('<3I',0x49000003|(kind<<16),0x24030001,0x24040002))
  taken=condition==(kind&1);annul=not taken and bool(kind&2)
  assert call('ee_core_step_n',1)==1
  assert int.from_bytes(u.mem_read(state+off['pc'],4),'big')==pc+(8 if annul else 4)
  assert call('ee_core_step_n',1)==1
  assert int.from_bytes(u.mem_read(state+off['pc'],4),'big')==pc+(16 if taken else 12 if annul else 8)
  assert int.from_bytes(u.mem_read(state+3*16,8),'big')==(0 if annul else 1)
  bc2_cases+=1

# The encoding-keyed PPC cache must use each live PC+4 region.
region_cases=0
for region_pc in (0x0ffffffc,0x8ffffffc,0xfffffffc):
 for opcode in (2,3):
  setup();word(state+off['exc_this_pc'],region_pc)
  word(state+off['pc'],(region_pc+4)&0xffffffff)
  word(state+off['next_pc'],(region_pc+8)&0xffffffff)
  word(syms['gekko2_optimization_mask'],initial_mask|1)
  assert call('ee_jit_try_execute_one_at',state,region_pc,(opcode<<26)|4)==1
  target=((region_pc+4)&0xf0000000)|16
  assert int.from_bytes(u.mem_read(state+off['next_pc'],4),'big')==target
  if opcode==3:assert int.from_bytes(u.mem_read(state+31*16,8),'big')==((region_pc+8)&0xffffffff)
  region_cases+=1

# Production VU1 runners (not synthetic VU-sized memory) drive the real GIF.
_,unitoff=layout('core/hw/vu.h','vu1_state_t',['vi','mem','micro','xgkick_pending','xgkick_addr','instructions_executed','pipeline'])
_,gifoff=layout('core/hw/gif.h','gif_state_t',['ctx1_xyoffset_x','gif_path1_transfers'])
unit=call('vu1_get_state');gifstate=call('gif_get_state');kick_cases=0
for start in (0,16368):
 setup();call('vu1_init');call('gif_init')
 word(unit+unitoff['vi']+4,start//16)
 tag=struct.pack('<4I',0x8001,1<<28,14,0)
 payload=struct.pack('<4I',0x6789,0,0x18,0) # XYOFFSET_1
 u.mem_write(unit+unitoff['mem']+start,tag)
 u.mem_write(unit+unitoff['mem']+((start+16)&16383),payload)
 u.mem_write(unit+unitoff['micro'],struct.pack('<4I',0x80000efc,0x400002ff,0x800003bf,0x2ff))
 call('vu1_exec_micro',0)
 assert int.from_bytes(u.mem_read(gifstate+gifoff['ctx1_xyoffset_x'],4),'big')==0x6789
 assert bytes(u.mem_read(unit+unitoff['xgkick_pending'],1))==b'\x00'
 assert int.from_bytes(u.mem_read(unit+unitoff['xgkick_addr'],4),'big')==((start+32)&16383)
 assert int.from_bytes(u.mem_read(unit+unitoff['instructions_executed'],8),'big')==2
 assert int.from_bytes(u.mem_read(gifstate+gifoff['gif_path1_transfers'],8),'big')==1
 kick_cases+=1
print(json.dumps({'bc2_linked_ppc_oracles':bc2_cases,'jump_region_ppc_oracles':region_cases,'production_xgkick_ppc_oracles':kick_cases,'all_passed':True,'physical_wii_verified':False}))
