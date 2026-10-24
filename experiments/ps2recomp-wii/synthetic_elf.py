#!/usr/bin/env python3
"""Own synthetic R5900 function, with symbols; no BIOS/game content."""
from pathlib import Path
import argparse,struct
ENTRY=0x00200000
# addiu v0,a0,-1; addu v0,v0,a1; xor v0,v0,a0;
# sll v0,v0,3; jr ra; addiu v0,v0,7 (real delay slot).
WORDS=[0x2482ffff,0x00451021,0x00441026,0x000210c0,0x03e00008,0x24420007]
def build():
    code=struct.pack('<6I',*WORDS);strings=b'\0probe\0';names=b'\0.text\0.symtab\0.strtab\0.shstrtab\0'
    textoff=0x100;syms_off=(textoff+len(code)+15)&~15
    syms=bytes(16)+struct.pack('<IIIBBH',1,ENTRY,len(code),0x12,0,1)
    str_off=syms_off+len(syms);names_off=str_off+len(strings);shoff=(names_off+len(names)+3)&~3
    data=bytearray(shoff+5*40)
    data[:16]=b'\x7fELF\x01\x01\x01'+bytes(9)
    struct.pack_into('<HHIIIIIHHHHHH',data,16,2,8,1,ENTRY,52,shoff,0,52,32,1,40,5,4)
    struct.pack_into('<8I',data,52,1,textoff,ENTRY,ENTRY,len(code),len(code),5,16)
    data[textoff:textoff+len(code)]=code;data[syms_off:syms_off+len(syms)]=syms
    data[str_off:str_off+len(strings)]=strings;data[names_off:names_off+len(names)]=names
    sections=[(0,)*10,(1,1,6,ENTRY,textoff,len(code),0,0,4,0),(7,2,0,0,syms_off,len(syms),3,1,4,16),(15,3,0,0,str_off,len(strings),0,0,1,0),(23,3,0,0,names_off,len(names),0,0,1,0)]
    for i,section in enumerate(sections):struct.pack_into('<10I',data,shoff+40*i,*section)
    return bytes(data)
def expected(a,b):
    value=((((a-1)&0xffffffff)+(b&0xffffffff))&0xffffffff)^(a&0xffffffff)
    value=((value<<3)+7)&0xffffffff
    return value if value<0x80000000 else value|0xffffffff00000000
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);a=p.parse_args();a.output.write_bytes(build())
