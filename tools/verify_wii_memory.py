#!/usr/bin/env python3
"""Reject Wii ELFs that place static data outside MEM1 or leave under 1 MiB."""
import argparse
import json
from pathlib import Path
import struct

def check(data, minimum_free=1024*1024):
    if len(data)<52 or data[:7]!=b'\x7fELF\x01\x02\x01':
        raise ValueError('expected big-endian ELF32')
    if struct.unpack_from('>H',data,18)[0]!=20:
        raise ValueError('expected PowerPC ELF')
    offset=struct.unpack_from('>I',data,28)[0]
    stride,count=struct.unpack_from('>HH',data,42)
    if stride<32 or offset+stride*count>len(data):
        raise ValueError('invalid program headers')
    high=0x80000000
    for index in range(count):
        kind,_,address,_,size,memory,_,_=struct.unpack_from('>8I',data,offset+stride*index)
        if kind!=1 or not memory:continue
        if memory<size:raise ValueError('invalid load segment sizes')
        if 0x80000000<=address<0x81800000:
            high=max(high,address+memory)
            if high>0x81800000:raise ValueError('static load segment exceeds Wii MEM1')
    free=0x81800000-high
    if high==0x80000000:raise ValueError('missing MEM1 load segments')
    if free<minimum_free:raise ValueError(f'only {free} bytes of MEM1 remain; require {minimum_free}')
    return {'mem1_static_end':hex(high),'mem1_remaining_bytes':free,'minimum_remaining_bytes':minimum_free,'passed':True}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('elf')
    args=parser.parse_args()
    try:print(json.dumps(check(Path(args.elf).read_bytes())))
    except (ValueError,OSError,struct.error) as error:parser.exit(1,f'Wii memory check: {error}\n')
