"""Test Wii Remote/Nunchuk mapping in the actual Wii ELF.
Bluetooth and physical pairing require real hardware; no emulated claim.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text(),'verify_ppc_gs_memory.py','exec'))
# Constants from the supplied libogc 1.8.18 headers (PAD and WPAD).
remote=[(0x8,0x100),(0x4,0x200),(0x2,0x400),(0x1,0x800),
        (0x1000,0x1000),(0x10,0x10),(0x800,8),(0x400,4),(0x100,1),(0x200,2)]
for button,pad in remote:
 assert call('wii_remote_to_pad',button,0,0,0)==pad,(button,pad)
assert call('wii_remote_to_pad',0x80,0,0,0)==0 # HOME never leaks into guest buttons
assert call('wii_remote_to_pad',0x30000,0,1,1)==0 # ignore foreign/missing expansion
assert call('wii_remote_to_pad',0x30000,1,0,0)==0x60 # C=L1, Z=R1
for sx in [-1,0,1]:
 for sy in [-1,0,1]:
  expected=(1 if sx<0 else 2 if sx>0 else 0)|(4 if sy<0 else 8 if sy>0 else 0)
  assert call('wii_remote_to_pad',0,1,sx&0xffffffff,sy&0xffffffff)==expected
for lo,center,hi in [(0,128,255),(30,120,210),(90,130,190)]:
 for pos in range(256):
  d=pos-center
  expected=1 if d>0 and d*100>(hi-center)*35 else -1 if d<0 and -d*100>(center-lo)*35 else 0
  assert call('wii_stick_direction',pos,lo,center,hi)==(expected&0xffffffff)
for values in [(0,0,0,0),(128,140,128,255),(128,0,128,120)]:
 assert call('wii_stick_direction',*values)==0
print('PASS actual Wii ELF Remote mappings, Nunchuk C/Z, diagonal stick, dead zone and invalid calibration; Bluetooth not tested')
