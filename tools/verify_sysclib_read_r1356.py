"""Linked Wii-PPC oracle: registered read-only IOP ABI, RAM bounds/fallbacks."""
from pathlib import Path
text=Path(__file__).with_name('verify_iop_service_hle_r1353.py').read_text()
exec(compile(text.split('start([0]*32')[0],'read_fixture','exec'))
start([0]*32,[0]*128,0);call('iop_service_hle_reset')
word(mask,initial|(1<<28)|(1<<14))
table=0x100400;u.mem_write(0x81750000,b'sysclib\0')
ordinals=[10,11,12,13,14,15,21,22,25,26,27,29,32]
for ordinal in range(33):
 pc=0x110000+ordinal*128
 u.mem_write(iram+table+ordinal*4,struct.pack('<I',pc if ordinal in ordinals else 0))
 u.mem_write(iram+pc,struct.pack('<16I',*([0x03e00008,0]+[0]*14)))
call('iop_service_hle_register',ist,0x81750000,table,33)
call('iop_service_hle_stat',0);assert u.reg_read(UC_PPC_REG_4)==13
checks=0
left=0x180000;right=0x181000

def invoke(ordinal,a,b,n,expected,accept=1):
 global checks
 pc=0x110000+ordinal*128
 word(ist+16,a);word(ist+20,b);word(ist+24,n);word(ist+124,0x100800)
 word(ist+io['pc'],pc);word(ist+io['next_pc'],pc+4);word(ist+8,0xfeedface)
 before=bytes(u.mem_read(ist,isize));memory=bytes(u.mem_read(iram,0x200000))
 assert call('iop_service_hle_try_handle',ist,pc)==accept,(ordinal,a,b,n)
 after=bytes(u.mem_read(ist,isize))
 if not accept:assert after==before
 else:
  assert int.from_bytes(after[8:12],'big')==expected&0xffffffff,(ordinal,a,b,n,after[8:12].hex(),expected)
  assert int.from_bytes(after[io['pc']:io['pc']+4],'big')==0x100800
  # Every caller GPR except the documented return register remains intact.
  assert after[12:128]==before[12:128]
 assert bytes(u.mem_read(iram,0x200000))==memory
 checks+=1

for segment in [0,0x80000000,0xa0000000]:
 a=left|segment;b=right|segment
 for x,y in [(b'abc\0',b'abc\0'),(b'\xff\0',b'\x01\0'),(b'a\0',b'ab\0'),(b'\0',b'z\0'),(b'z\0',b'\0')]:
  u.mem_write(iram+left,x);u.mem_write(iram+right,y)
  diff=next((xx-yy for xx,yy in zip(x,y) if xx!=yy),0)
  invoke(22,a,b,0,diff)
  for n in [0,1,2,min(len(x),len(y))]:
   expected=next((xx-yy for xx,yy in zip(x[:n],y[:n]) if xx!=yy),0)
   for ordinal in [11,15,29]:invoke(ordinal,a,b,n,expected)
 for data in [b'\0',b'abca\0',b'\xff\x80\xff\0']:
  u.mem_write(iram+left,data);invoke(27,a,0,0,len(data)-1)
  for c in [0,ord('a'),0xff,ord('z'),0x180]:
   positions=[i for i,x in enumerate(data) if x==(c&255)]
   first=a+positions[0] if positions else 0;last=a+positions[-1] if positions else 0
   for ordinal in [21,25]:invoke(ordinal,a,c,0,first)
   for ordinal in [26,32]:invoke(ordinal,a,c,0,last)
   invoke(10,a,c,len(data),first)
# RAM end, absent terminator, bounded prefix and zero-byte invalid pointers.
u.mem_write(iram+0x1ffffc,b'abc\0');invoke(27,0x1ffffc,0,0,3)
u.mem_write(iram+0x1ffffc,b'abcd');invoke(27,0x1ffffc,0,0,0,0)
u.mem_write(iram+left,b'abcd');invoke(29,0x1ffffc,left,4,0)
for ordinal in [10,11,15,29]:invoke(ordinal,0x1f801070,0x1f801070,0,0)
for ordinal in [10,11,15,29]:invoke(ordinal,left,right,65537,0,0)
for ordinal in [10,11,15]:invoke(ordinal,0x1ffffc,right,5,0,0)
for ordinal in [21,22,25,26,27,29,32]:invoke(ordinal,0x1f801070,right,4,0,0)
u.mem_write(iram+left,b'x'*65536);invoke(27,left,0,0,0,0)
u.mem_write(iram+left,b'x'*65535+b'\0');invoke(27,left,0,0,65535)
for flag in [14,28]:
 word(mask,(initial|(1<<28)|(1<<14))&~(1<<flag));invoke(27,left,0,0,0,0)
word(mask,initial|(1<<28)|(1<<14));pc=0x110000+27*128
u.mem_write(iram+pc,bytes(4));invoke(27,left,0,0,0,0)
# SIO2 remains registered after sysclib replacement (30 total services).
u.mem_write(0x81750010,b'sio2man\0');table2=0x100800
for ordinal in range(21):u.mem_write(iram+table2+ordinal*4,struct.pack('<I',0x120000+ordinal*128 if ordinal>=4 else 0))
call('iop_service_hle_register',ist,0x81750010,table2,21)
call('iop_service_hle_register',ist,0x81750000,table,33)
call('iop_service_hle_stat',0);assert u.reg_read(UC_PPC_REG_4)==30
print('PASS',checks,'linked read-only sysclib ABI cases; exact caller GPRs, RAM unchanged, signed bytes, aliases, bounds/caps, source/option fallback; 30-service replacement registry')
