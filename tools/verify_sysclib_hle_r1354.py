"""Registered sysclib RAM ABI and memory/fallback oracle in linked PPC."""
from pathlib import Path
text=Path(__file__).with_name('verify_iop_service_hle_r1353.py').read_text()
exec(compile(text.split('start([0]*32')[0],'sysclib_fixture','exec'))
start([0]*32,[0]*128,0);call('iop_service_hle_reset')
word(mask,initial|(1<<28)|(1<<14))
table=0x100400;u.mem_write(0x81750000,b'sysclib\0')
for ordinal in range(15):
 pc=0x110000+ordinal*128
 u.mem_write(iram+table+ordinal*4,struct.pack('<I',pc if ordinal>=12 else 0))
 u.mem_write(iram+pc,struct.pack('<16I',*([0x03e00008,0]+[0]*14)))
call('iop_service_hle_register',ist,0x81750000,table,15)
call('iop_service_hle_stat',0);assert u.reg_read(UC_PPC_REG_4)==3
checks=0
def args(ordinal,dst,src,n):
 pc=0x110000+ordinal*128
 word(ist+16,dst);word(ist+20,src);word(ist+24,n);word(ist+124,0x100800)
 word(ist+io['pc'],pc);word(ist+io['next_pc'],pc+4);word(ist+8,0xfeedface)
 return pc
for ordinal in [12,13,14]:
 for n in [0,1,63,64,257,4096,65536]:
  seed=bytes(range(256))*512;u.mem_write(iram+0x180000,seed)
  for segment in [0,0x80000000,0xa0000000]:
   expected=bytearray(seed)
   dst=0x190000;src=0x180000
   if ordinal==14:src=0x1234ab;expected[0x10000:0x10000+n]=bytes([0xab])*n
   else:expected[0x10000:0x10000+n]=expected[:n]
   pc=args(ordinal,dst|segment,src if ordinal==14 else src|segment,n)
   assert call('iop_service_hle_try_handle',ist,pc)==1
   assert bytes(u.mem_read(iram+0x180000,len(seed)))==expected
   assert int.from_bytes(u.mem_read(ist+8,4),'big')==dst|segment
   assert int.from_bytes(u.mem_read(ist+io['pc'],4),'big')==0x100800
   u.mem_write(iram+0x180000,seed);checks+=1
for delta in [-16,0,16]:
 seed=bytes(range(256))*32;u.mem_write(iram+0x180000,seed);expected=bytearray(seed)
 src=0x180100;dst=src+delta;n=128
 expected[dst-0x180000:dst-0x180000+n]=bytes(expected[0x100:0x100+n])
 pc=args(13,dst,src,n);assert call('iop_service_hle_try_handle',ist,pc)==1
 assert bytes(u.mem_read(iram+0x180000,len(seed)))==expected;checks+=1
for ordinal,dst,src,n in [(12,0x180010,0x180000,128),(12,0x1f801070,0x180000,64),(12,0x180000,0x1f801070,64),(14,0x1ffff0,0,64),(13,0x180000,0x180100,65537),(14,0x110000+14*128,0,64)]:
 pc=args(ordinal,dst,src,n);before=bytes(u.mem_read(ist,isize))
 assert call('iop_service_hle_try_handle',ist,pc)==0
 assert bytes(u.mem_read(ist,isize))==before;checks+=1
pc=args(12,0x181000,0x180000,64);word(mask,initial&~(1<<28))
before=bytes(u.mem_read(ist,isize));assert call('iop_service_hle_try_handle',ist,pc)==0
assert bytes(u.mem_read(ist,isize))==before
word(mask,initial|(1<<28)|(1<<14));u.mem_write(iram+pc,bytes(4))
before=bytes(u.mem_read(ist,isize));assert call('iop_service_hle_try_handle',ist,pc)==0
assert bytes(u.mem_read(ist,isize))==before
print('PASS',checks,'sysclib RAM/alias/overlap/bounds ABI cases plus disabled/changed-source fallbacks')
# Adding SIO2 and replacing sysclib preserves both independent registries.
u.mem_write(0x81750010,b'sio2man\0');table2=0x100800
for ordinal in range(21):
 u.mem_write(iram+table2+ordinal*4,struct.pack('<I',0x120000+ordinal*128 if ordinal>=4 else 0))
call('iop_service_hle_register',ist,0x81750010,table2,21)
call('iop_service_hle_register',ist,0x81750000,table,15)
call('iop_service_hle_stat',0);assert u.reg_read(UC_PPC_REG_4)==20
print('PASS SIO2 plus sysclib registration and replacement preserve all20 distinct services')

# Model IOS transport only; execute the real ARM-RAM validation/fallback glue.
transport={'cursor':0,'fail':False}
ios={syms[n]:n for n in ['IOS_Open','IOS_Close','IOS_Seek','IOS_Read','IOS_Write']}
def ios_hook(uc,address,size,user):
 name=ios.get(address)
 if not name:return
 r=0
 if name=='IOS_Open':r=7
 elif name=='IOS_Seek':transport['cursor']=uc.reg_read(UC_PPC_REG_4)
 elif name in ['IOS_Read','IOS_Write']:
  ptr=uc.reg_read(UC_PPC_REG_4);n=uc.reg_read(UC_PPC_REG_5)
  memory=transport['cursor']|0x80000000
  if name=='IOS_Read':uc.mem_write(ptr,bytes(uc.mem_read(memory,min(n,32) if transport['fail'] else n)))
  else:uc.mem_write(memory,bytes(uc.mem_read(ptr,min(n,32) if transport['fail'] else n)))
  r=-5 if transport['fail'] else 1
 uc.reg_write(UC_PPC_REG_3,r&0xffffffff);uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,ios_hook)
word(mask,initial|(1<<28)|(1<<14)|(1<<23))
for fail in [False,True]:
 for ordinal in [12,13,14]:
  call('arm_ram_reset');transport['fail']=fail
  seed=bytes(range(256))*512;u.mem_write(iram+0x180000,seed);expected=bytearray(seed)
  source=0x1234ab if ordinal==14 else 0x180000
  expected[0x10000:0x20000]=bytes([0xab])*65536 if ordinal==14 else seed[:65536]
  pc=args(ordinal,0x190000,source,65536)
  assert call('iop_service_hle_try_handle',ist,pc)==1
  assert bytes(u.mem_read(iram+0x180000,len(seed)))==expected
  call('arm_ram_stat',1);assert u.reg_read(UC_PPC_REG_4)>0
  call('arm_ram_stat',2);assert u.reg_read(UC_PPC_REG_4)==(0 if fail else 1)
print('PASS 6 registered64KiB ARM copy/fill jobs: verified transport and complete CPU fallback after partial failures')
