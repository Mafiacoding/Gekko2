"""Actual big-endian ARM worker fills with word-only shared-bus validation."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_arm_service_r1370.py').read_text().replace('count=2000000','count=12000000'),'arm-base','exec'))
u.mem_map(0x10100000,0x200000)
dest=0x10110000
for n in (65536,1048576):
 for value in (0,0x7b,0xff):
  u.mem_write(dest-32,b'\xa5'*(n+64));words(header,0x474b4131,1,value,0,0,0,0,0)
  words(vectors,header,32,dest,n);words(msg,7,0,0,2,1,1,vectors)
  assert call('gekko2_ios_handle',msg)==n
  assert bytes(u.mem_read(dest,n))==bytes([value])*n
  assert bytes(u.mem_read(dest-32,32))==b'\xa5'*32 and bytes(u.mem_read(dest+n,32))==b'\xa5'*32
for address,n in [(dest+1,65536),(dest,65535),(dest,32),(dest,1048608),(0x13700000,65536),(0xffff0000,65536),(header,65536)]:
 before=bytes(u.mem_read(dest,65536));words(vectors,header,32,address,n)
 assert call('gekko2_ios_handle',msg)==0xffffff9b
 assert bytes(u.mem_read(dest,65536))==before
print('PASS ARM RAM fill: six complete 64-KiB/1-MiB jobs, arbitrary byte patterns, untouched guards; seven malformed/IOS-private/overlap requests rejected atomically')
