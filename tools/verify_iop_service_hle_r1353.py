"""Execute registered SIO2 service gates in the actual linked PPC core."""
from pathlib import Path
text=Path(__file__).with_name('verify_event_scheduler_r1353.py').read_text()
exec(compile(text.split("ee=[ins(9")[0],'service_fixture','exec'))
start([0]*32,[0]*128,0);call('iop_sio2_init');call('iop_service_hle_reset')
u.mem_write(0x81750000,b'sio2man\0')
table=0x100400
for ordinal in range(21):
 address=0x110000+ordinal*128
 u.mem_write(iram+table+ordinal*4,struct.pack('<I',address if ordinal>=4 else 0))
 u.mem_write(iram+address,struct.pack('<16I',*(([0x03e00008,0]+[0]*14))))
call('iop_service_hle_register',ist,0x81750000,table,21)
call('iop_service_hle_stat',0);assert u.reg_read(UC_PPC_REG_4)==17
word(mask,initial|(1<<28));checks=0
for ordinal in range(4,21):
 for index in ([0,3] if ordinal in [7,8,9,10] else [0,15] if ordinal in [12,13] else [0]):
  pc=0x110000+ordinal*128;arg=0x3bc if ordinal==4 else 0x1234abcd
  word(ist+16,index if ordinal in [7,8,9,10,12,13] else arg);word(ist+20,0x87654321)
  word(ist+124,0x100800);word(ist+io['pc'],pc);word(ist+io['next_pc'],pc+4);word(ist+8,0xfeedface)
  address={4:0x1f808268,5:0x1f808268,6:0x1f80826c,11:0x1f808270,14:0x1f808274,15:0x1f808278,16:0x1f808278,17:0x1f80827c,18:0x1f80827c,19:0x1f808260,20:0x1f808264}.get(ordinal)
  if ordinal in [7,8,9,10]:address=0x1f808240+index*8+(4 if ordinal>=9 else 0)
  if ordinal in [12,13]:address=0x1f808200+index*4
  write=ordinal in [4,7,9,12,15,17,19]
  if not write:call('iop_mem_write32',ist,address,0x87654321)
  before=call('iop_mem_read8' if ordinal==20 else 'iop_mem_read32',ist,address) if not write else 0
  assert call('iop_service_hle_try_handle',ist,pc)==1,(ordinal,index)
  assert int.from_bytes(u.mem_read(ist+io['pc'],4),'big')==0x100800
  if write:
   assert int.from_bytes(u.mem_read(ist+8,4),'big')==0xfeedface
   if ordinal!=19:assert call('iop_mem_read32',ist,address)==(0x87654321 if ordinal in [7,9,12] else arg)
  else:assert int.from_bytes(u.mem_read(ist+8,4),'big')==before,(ordinal,index)
  checks+=1
# Disabled / changed function / invalid indexed argument leave guest intact.
pc=0x110000+7*128;word(ist+io['pc'],pc);word(ist+io['next_pc'],pc+4)
for mode in ['off','invalid_index','changed_source']:
 word(mask,initial|(1<<28));word(ist+16,0)
 if mode=='off':word(mask,initial&~(1<<28))
 if mode=='invalid_index':word(ist+16,4)
 if mode=='changed_source':u.mem_write(iram+pc,struct.pack('<I',0))
 before=bytes(u.mem_read(ist,isize));assert call('iop_service_hle_try_handle',ist,pc)==0
 assert bytes(u.mem_read(ist,isize))==before
print('PASS',checks,'registered native SIO2 ABI calls plus 3 pure fallbacks')
# Actual IOP core route: registered function consumes one real HLE tick.
word(mask,initial|(1<<28));pc=0x110000+5*128
word(ist+io['pc'],pc);word(ist+io['next_pc'],pc+4);word(ist+124,0x100800)
assert call('iop_core_step_n',1)==1
assert int.from_bytes(u.mem_read(ist+io['pc'],4),'big')==0x100800
assert int.from_bytes(u.mem_read(ist+io['instructions_executed'],8),'big')==1
print('PASS actual IOP scheduler route to native service')
