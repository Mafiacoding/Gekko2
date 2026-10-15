"""Complete VU state/memory oracle for re-entry after scheduler boundaries."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
fields0=['vu0_vf','cop2_ctrl','vu0_acc','vu0_mem','vu0_micro','vu0_branch_delay','vu0_branch_target','vu0_ebit_delay','vu0_instructions_executed','vu0_pipeline']
fields1=['vf','vi','acc','mem','micro','tpc','branch_delay','branch_target','ebit_delay','instructions_executed','pipeline']
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'layout.c';obj=Path(td)/'layout.o';raw=Path(td)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/ee/ee_core.h"\n#include "core/hw/vu.h"\nconst unsigned x[]={sizeof(vu1_state_t),'+','.join('offsetof(ee_state_t,'+f+')' for f in fields0)+','+','.join('offsetof(vu1_state_t,'+f+')' for f in fields1)+',sizeof(vu_pipeline_t)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 vals=struct.unpack('>23I',raw.read_bytes());vsize=vals[0];v0=dict(zip(fields0,vals[1:11]));v1=dict(zip(fields1,vals[11:22]));pipeline_size=vals[-1]
vs=call('vu1_get_state');initial=int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')
NOP=(11<<6)|63
ADD=(15<<21)|(1<<16)|(3<<11)|(3<<6)|40
LOW=0x80000000|(2<<16)|(1<<11)|(3<<6)|48
gif_events=[]
block_compiles=[0]
def gif_consumer(uc,address,size,user):
 if address==syms.get('ppc_dynarec_translate_vu_block'):block_compiles[0]+=1
 if address!=syms.get('gif_process_quadwords'):return
 path=uc.reg_read(UC_PPC_REG_3);data=uc.reg_read(UC_PPC_REG_4);qwc=uc.reg_read(UC_PPC_REG_5)
 def rd(f,n=4):return int.from_bytes(uc.mem_read(vs+v1[f],n),'big')
 gif_events.append((path,qwc,hashlib.sha256(bytes(uc.mem_read(data,qwc*16))).hexdigest(),rd('tpc'),rd('instructions_executed',8),rd('pipeline',8)))
 uc.reg_write(UC_PPC_REG_3,0);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,gif_consumer)
checks=0;measures={}
for unit in [0,1]:
 for boundary in ['branch','Q','P','E_continue','branch_short','xgkick','xgkick_wrap']:
  if unit==0 and boundary=='P':continue
  for route in ['control','bundled','interpreter']:
   mask=initial|(1<<2)|(1<<20)
   if route=='control':mask&=~(1<<20)
   if route=='interpreter':mask&=~(1<<2)
   word(syms['gekko2_optimization_mask'],mask)
   extension_setup();call('vu1_init');call('vu_jit_reset_for_test')
   if unit==0:
    view={k:state+v0[f] for k,f in zip(['vf','vi','acc','mem','micro','branch_delay','branch_target','ebit_delay','instructions_executed','pipeline'],fields0)}
    view['tpc']=view['vi']+26*4;entry='vu0_exec_micro';cont='vu0_exec_micro_continue'
   else:view={k:vs+v1[k] for k in fields1};entry='vu1_exec_micro';cont='vu1_exec_micro_continue'
   for k in range(4):word(view['vf']+16+k*4,0x3f800000)
   word(view['vi']+4,7);word(view['vi']+8,9)
   program=[(0,NOP)]*3
   if boundary.startswith('branch') or boundary.startswith('xgkick'):program[0]=((32<<25)|2,NOP)
   if boundary=='Q':program[0]=(0x80000000|(2<<16)|(1<<11)|(14<<6)|60,NOP)
   if boundary=='P':program[0]=(0x80000000|(1<<11)|(28<<6)|60,NOP)
   if boundary=='E_continue':
    program[0]=(0,NOP|0x40000000)
    program[2]=((32<<25)|0,NOP)
   length=2 if boundary=='branch_short' else 32
   program += [(LOW,ADD)]*length+[(0,NOP|0x40000000),(0,NOP)]
   if boundary.startswith('xgkick'):program[13]=(0x80000000|(5<<11)|(0x1b<<6)|60,NOP)
   def write_program():u.mem_write(view['micro'],b''.join(struct.pack('<2I',l,h) for l,h in program))
   write_program()
   # Warm native owners, then reset architectural state only. Repeated calls
   # retain the real code cache while reproducing the same VU workload.
   call(entry,state,0) if unit==0 else call(entry,0)
   if boundary=='E_continue':call(cont,state) if unit==0 else call(cont)
   snaps=[];cost=0;warm_compiles=0
   for repeat in range(4):
    if unit==0:
     u.mem_write(state+v0['vu0_vf'],bytes(512));u.mem_write(state+v0['cop2_ctrl'],bytes(128));u.mem_write(state+v0['vu0_acc'],bytes(16));u.mem_write(state+v0['vu0_mem'],bytes(4096))
     for k in ['branch_delay','branch_target','ebit_delay']:word(view[k],0)
     u.mem_write(view['instructions_executed'],bytes(8));u.mem_write(view['pipeline'],bytes(pipeline_size))
    else:call('vu1_init')
    write_program()
    for k in range(4):word(view['vf']+16+k*4,0x3f800000)
    word(view['vi']+4,7);word(view['vi']+8,9)
    if boundary.startswith('xgkick'):
     word(view['vi']+5*4,1023 if boundary.endswith('wrap') else 0)
     u.mem_write(view['mem'],bytes(range(256))*(64 if unit else 16))
    gif_events.clear();block_compiles[0]=0
    total[0]=0;active[0]=True
    call(entry,state,0) if unit==0 else call(entry,0)
    if boundary=='E_continue':call(cont,state) if unit==0 else call(cont)
    active[0]=False;cost+=total[0];warm_compiles+=block_compiles[0]
    snap=bytes(u.mem_read(state,size)) if unit==0 else bytes(u.mem_read(vs,vsize))
    if boundary.startswith('xgkick'):assert len(gif_events)==(1 if unit else 0),(unit,boundary,route,gif_events)
    else:assert not gif_events
    snaps.append((hashlib.sha256(snap).hexdigest(),list(gif_events)))
   if route=="bundled":assert warm_compiles==0,(unit,boundary,warm_compiles)
   key=(unit,boundary)
   if key in measures:assert measures[key][0]==snaps,(key,route,measures[key][0],snaps)
   else:measures[key]=(snaps,{})
   measures[key][1][route]=cost
   checks+=1
word(syms['gekko2_optimization_mask'],initial)
for key,(snaps,cost) in measures.items():print('VU_BATCH',key,cost,flush=True)
print('PASS',checks,'VU0/VU1 branch/Q/P/E-continue/XGKICK complete-state routes and warm PPC cost comparisons')
