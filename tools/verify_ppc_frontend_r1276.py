"""Execute the final Wii font renderer and image-name filter on PPC.
Directory I/O and controller navigation are covered by host fixtures/build only.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_vu0_r1273.py').read_text(),'verify_ppc_vu0_r1273.py','exec'))
u.mem_map(0x90900000,1024*1024)
dest=0x90901000;text=0x909a0000;words=640*480//2
base=call('gs_rgb8_pair_to_ycbcr',20,40,60,20,40,60)
u.mem_write(dest,struct.pack('>I',base)*words)
word(dest-4,0x12345678);word(dest+words*4,0xabcdef01)
u.mem_write(text,b'PCSX2\0')
word(0x81780000+8,240);word(0x81780000+12,240)
call('frontend_text_draw',dest,640,480,51,30,3,text,240)
output=struct.unpack('>'+str(words)+'I',bytes(u.mem_read(dest,words*4)))
ink=call('gs_rgb8_pair_to_ycbcr',240,240,240,240,240,240)
assert sum(v!=base for v in output)>100
assert sum(v not in (base,ink) for v in output)>100
assert bytes(u.mem_read(dest-4,4))==bytes.fromhex('12345678')
assert bytes(u.mem_read(dest+words*4,4))==bytes.fromhex('abcdef01')
for name,expected in [(b'Tekken Tag.BIN\0',1),(b'another.ISO\0',1),(b'X.bin.exe\0',0),(b'folder\0',0)]:
 u.mem_write(text,name);assert call('frontend_browser_is_image',text)==expected
print('PASS actual Wii ELF native coverage font, antialiased pixels, XFB bounds and case-insensitive ISO/BIN filter')
