#!/usr/bin/env python3
"""Synthetic ROMDIR bounds tests; no BIOS bytes or derived assets."""
from pathlib import Path
import importlib.util,struct
root=Path(__file__).resolve().parents[1];s=importlib.util.spec_from_file_location('bios',root/'tools/investigate_bios.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
def entry(name,size):return struct.pack('<10sHI',name.encode(),0,size)
data=bytearray(4*1024*1024);data[32:64]=entry('RESET',32)+entry('ROMDIR',64)
rows=m.inventory(data);assert len(rows)==2 and rows[1]['offset']==32
checks=1
for malformed in (data[:-1],bytes(len(data)),data[:32]+entry('RESET',32)+entry('ROMDIR',len(data)+1)+data[64:],data[:32]+entry('RESET',64)+entry('ROMDIR',64)+data[64:]):
 try:m.inventory(malformed)
 except ValueError:checks+=1
 else:raise AssertionError('malformed directory accepted')
print(f'PASS {checks} synthetic BIOS directory bounds and offset checks; no proprietary fixture')
