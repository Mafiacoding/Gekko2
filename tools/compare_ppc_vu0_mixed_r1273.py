"""Count actual PPC instructions in an identical warm VU1 microprogram.
Instruction count is not a hardware FPS/cycle benchmark.
"""
import argparse,sys,struct,subprocess,tempfile,json
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.ppc_const import *
p=argparse.ArgumentParser();p.add_argument('before');p.add_argument('after');p.add_argument('--nm',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];cc=str(Path(a.nm).with_name('powerpc-eabi-gcc'))
with tempfile.TemporaryDirectory() as directory:
 src=Path(directory)/'layout.c';obj=Path(directory)/'layout.o';raw=Path(directory)/'layout.bin'
 fields=['vf','vi','acc','mem','micro','instructions_executed','unimplemented_opcodes_seen','tpc','branch_delay','branch_target','ebit_delay','running']
 fieldmap={'vf':'vu0_vf','vi':'cop2_ctrl','acc':'vu0_acc','mem':'vu0_mem','micro':'vu0_micro','instructions_executed':'vu0_instructions_executed','unimplemented_opcodes_seen':'vu0_unimplemented_opcodes_seen','branch_delay':'vu0_branch_delay','branch_target':'vu0_branch_target','ebit_delay':'vu0_ebit_delay','running':'vu0_running'}
 src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\nconst unsigned layout[]={'+','.join(('offsetof(ee_state_t,cop2_ctrl)+26*sizeof(uint32_t)' if f=='tpc' else 'offsetof(ee_state_t,'+fieldmap[f]+')') for f in fields)+'};\n')
 subprocess.run([cc,'-O2','-msdata=none','-I'+str(r/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([str(Path(a.nm).with_name('powerpc-eabi-objcopy')),'-O','binary','--only-section=.rodata',str(obj),str(raw)],check=True)
 offsets=struct.unpack('>'+str(len(fields))+'I',raw.read_bytes());voff=dict(zip(fields,offsets))
loader=(r/'tools/verify_ppc_gs_memory.py').read_text().split('checks=0')[0]
results=[];final_states=[]
for elf in [a.before,a.after]:
 sys.argv=['loader',elf,'--nm',a.nm];g={};exec(compile(loader,'ELF loader','exec'),g)
 u=g['u'];call=g['call'];syms=g['syms'];u.reg_write(UC_PPC_REG_MSR,0x2000)
 heap=[0x81600000];total=[0];counting=[False]
 def hook(uc,address,size,data):
  if counting[0]:total[0]+=1
  if address==syms.get('memalign'):
   alignment=uc.reg_read(UC_PPC_REG_3);size=uc.reg_read(UC_PPC_REG_4);ptr=(heap[0]+alignment-1)&-alignment;heap[0]=ptr+size
   assert heap[0]<0x81700000
   uc.reg_write(UC_PPC_REG_3,ptr);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
  elif address in [syms.get(n) for n in ['free','DCFlushRange','ICInvalidateRange']]:uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
 u.hook_add(UC_HOOK_CODE,hook);u.ctl_remove_cache(0x80000000,0x81800000)
 vs=call('ee_core_get_state')
 def word(addr,v):u.mem_write(addr,struct.pack('>I',v))
 def setup():
  for field,size in [('vf',512),('vi',128),('acc',16),('mem',4096),('micro',4096),('instructions_executed',8),('unimplemented_opcodes_seen',8),('branch_delay',4),('branch_target',4),('ebit_delay',4),('running',1)]:u.mem_write(vs+voff[field],bytes(size))
  vf=vs+voff['vf'];vi=vs+voff['vi'];acc=vs+voff['acc']
  for k in range(4):word(vf+16+k*4,0x3f800000);word(vf+32+k*4,0x40000000);word(acc+k*4,0x40400000)
  word(vi+4,20)
  pairs=[((0x40<<25)|(1<<16)|(1<<11)|(31<<6)|0x32,(15<<21)|(2<<16)|(1<<11)|(3<<6)|0x28)]
  pairs += [((0x40<<25)|(2<<16)|(1<<11)|(3<<6)|0x30,(15<<21)|(2<<16)|(1<<11)|(4<<6)|0x28)]*7
  pairs += [((0x29<<25)|(1<<11)|0x7f7,(15<<21)|(2<<16)|(1<<11)|(5<<6)|0x2a),((0x40<<25)|(15<<21)|(6<<16)|(3<<11)|(12<<6)|0x3c,(15<<21)|(2<<16)|(1<<11)|(7<<6)|0x2c),(0x800003bf,0x400002ff),(0x800003bf,0x2ff)]
  for n,(lo,hi) in enumerate(pairs):u.mem_write(vs+voff['micro']+n*8,struct.pack('<2I',lo,hi))
 setup();call('vu0_exec_micro',vs,0) # populate generated cache
 samples=[]
 for iteration in range(3):
  setup();total[0]=0;counting[0]=True;call('vu0_exec_micro',vs,0);counting[0]=False;samples.append(total[0])
  assert int.from_bytes(bytes(u.mem_read(vs+voff['instructions_executed'],8)),'big')==202
  assert int.from_bytes(bytes(u.mem_read(vs+voff['unimplemented_opcodes_seen'],8)),'big')==0
 assert len(set(samples))==1,samples
 final_states.append(b''.join(bytes(u.mem_read(vs+voff[field],size)) for field,size in [('vf',512),('vi',128),('acc',16),('mem',4096)]))
 results.append({'elf':Path(elf).name,'warm_ppc_instructions':samples[0],'retired_vu_pairs':202,'samples':samples})
assert final_states[0]==final_states[1],'VU state differs between old and new backend'
change=(results[1]['warm_ppc_instructions']/results[0]['warm_ppc_instructions']-1)*100
print(json.dumps({'results':results,'instruction_change_percent':change,'state_identical':True,'scope':'Synthetic warm VU0 loop; actual PPC instruction count, not Wii FPS, cycles or OSDSYS input latency.'},indent=2))
(r/'outputs/verification/vu_instruction_comparison_r1273_vu0_mixed.json').write_text(json.dumps({'results':results,'instruction_change_percent':change,'state_identical':True,'scope':'Synthetic warm VU0 loop; actual PPC instruction count, not Wii FPS, cycles or OSDSYS input latency.'},indent=2))
