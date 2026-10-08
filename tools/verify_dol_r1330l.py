"""Validate DOL sections against ELF bytes and Wii startup signatures.
This is a file-format check, not a Dolphin or physical Wii boot test.
"""
import argparse, struct, hashlib, json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('dol');p.add_argument('elf');a=p.parse_args()
d=Path(a.dol).read_bytes();e=Path(a.elf).read_bytes()
assert e[:6]==b'\x7fELF\x01\x02' and len(d)>=256
entry=struct.unpack_from('>I',e,24)[0]
phoff=struct.unpack_from('>I',e,28)[0];stride,count=struct.unpack_from('>HH',e,42)
segments=[]
for i in range(count):
 kind,off,addr,_,size,memsize,flags,align=struct.unpack_from('>8I',e,phoff+i*stride)
 if kind==1 and size:segments.append((off,addr,size))
offsets=struct.unpack_from('>18I',d,0);addresses=struct.unpack_from('>18I',d,72);sizes=struct.unpack_from('>18I',d,144)
sections=[];markers=[]
for off,addr,size in zip(offsets,addresses,sizes):
 if not size:continue
 assert off>=256 and off+size<=len(d)
 assert 0x80000000<=addr<addr+size<=0x81800000
 matches=[(f,v) for f,v,n in segments if v<=addr and addr+size<=v+n]
 assert len(matches)==1,(hex(addr),size,matches)
 f,v=matches[0];assert d[off:off+size]==e[f+addr-v:f+addr-v+size]
 for start,end in sections:assert addr+size<=start or addr>=end
 sections.append((addr,addr+size))
 for i in range(0,size-3,4):
  if struct.unpack_from('>I',d,off+i)[0]&0xfc1fffff==0x7c13fba6:markers.append(hex(addr+i))
bss,bss_size,dentry=struct.unpack_from('>3I',d,216)
assert dentry==entry==0x80004000 and any(start<=entry<end for start,end in sections)
assert bss_size and 0x80000000<=bss<bss+bss_size<=0x81800000
assert markers,'Wii startup HID4 signature missing'
print(json.dumps({'dol':Path(a.dol).name,'sha256':hashlib.sha256(d).hexdigest(),'bytes':len(d),'entry':hex(entry),'sections':len(sections),'load_end':hex(max(end for _,end in sections)),'bss_end':hex(bss+bss_size),'wii_hid4_markers':markers,'elf_section_bytes_match':True,'runtime_boot_verified':False},indent=2))
