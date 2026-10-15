#!/usr/bin/env python3
"""Compile VU instruction words against shared upper/lower C bodies."""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct

def generate(upper,lower):
    out=['/* GPL-3.0-or-later. Locally generated, exact-word VU specializations. */']
    signatures={'upper':'uint32_t vf[32][4],uint32_t *vi,uint32_t acc[4]', 'lower':'uint32_t vf[32][4],uint32_t *vi,uint8_t *mem,uint32_t mem_mask,uint32_t pc,uint32_t *branch_delay,uint32_t *branch_target'}
    arguments={'upper':'vf,vi,acc', 'lower':'vf,vi,mem,mem_mask,pc,branch_delay,branch_target'}
    for kind,words in [('upper',sorted(set(w&0x01ffffff for w in upper))),('lower',sorted(set(lower)))]:
        for i,w in enumerate(words):
            out += [f'static int vu_aot_{kind}_{i}({signatures[kind]}) {{',f'const uint32_t w=UINT32_C(0x{w:08x});',f'#include "hw/vu_{kind}_body.inc"','}']
        out += [f'static int vu_aot_has_{kind}(uint32_t word) {{','switch(word) {']
        out += [f'case UINT32_C(0x{w:08x}):return 1;' for w in words]
        out += ['default:return 0;','}','}',f'static int vu_aot_try_{kind}({signatures[kind]},uint32_t word) {{', 'if(!gekko2_opt_enabled(GEKKO2_OPT_VU_JIT))return 0;']
        if kind=='upper':out+=['word&=0x01ffffffu;']
        out += ['int ok=0;switch(word) {']
        out += [f'case UINT32_C(0x{w:08x}):ok=vu_aot_{kind}_{i}({arguments[kind]});break;' for i,w in enumerate(words)]
        out += ['default:return 0;','}',f'if(ok)vu_aot_counts[{0 if kind=="upper" else 1}]++;return ok;','}']
    # The issue scheduler owns flag latency; each hot arithmetic word has
    # a compiler-specialized native datapath returning its MAC/STATUS result.
    out += ['static int vu_aot_try_timed_upper(uint32_t vf[32][4],uint32_t *vi,uint32_t acc[4],uint32_t word,uint32_t *mac,uint32_t *status) {',
            'if(!gekko2_opt_enabled(GEKKO2_OPT_VU_JIT))return 0;',
            'int ok=0;switch(word&0x01ffffffu) {']
    out += [f'case UINT32_C(0x{w:08x}):ok=vu_fmac_arithmetic(vf,vi,acc,UINT32_C(0x{w:08x}),mac,status);break;' for w in sorted(set(w&0x01ffffff for w in upper))]
    out += ['default:return 0;','}','if(ok)vu_aot_counts[0]++;return ok;','}']
    out += ['static int vu_aot_available(uint32_t upper,uint32_t lower) {return gekko2_opt_enabled(GEKKO2_OPT_VU_JIT)&&(vu_aot_has_upper(upper&0x01ffffffu)||(!(upper&0x80000000u)&&vu_aot_has_lower(lower)));}','']
    return '\n'.join(out)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    group=parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--micro',action='append',help='local VU0/VU1 micro image; may repeat')
    group.add_argument('--manifest',help='JSON arrays upper/lower of raw uint32 words')
    parser.add_argument('--max-words',type=int,default=64)
    parser.add_argument('--output',required=True)
    args=parser.parse_args()
    try:
        if not 1<=args.max_words<=256:raise ValueError('max-words must be 1..256')
        upper,lower=Counter(),Counter()
        if args.manifest:
            data=json.loads(Path(args.manifest).read_text())
            for kind,target in [('upper',upper),('lower',lower)]:
                for value in data[kind]:
                    w=int(value,0) if isinstance(value,str) else int(value)
                    if not 0<=w<=0xffffffff:raise ValueError('word outside uint32 range')
                    target[w]+=1
        else:
            for filename in args.micro:
                data=Path(filename).read_bytes()
                if len(data) not in (4096,16384):raise ValueError('VU micro image must contain 4 KiB or 16 KiB')
                for lo,hi in struct.iter_unpack('<II',data):
                    upper[hi&0x01ffffff]+=1
                    if not hi&0x80000000:lower[lo]+=1
        ups=[w for w,_ in upper.most_common(args.max_words)];los=[w for w,_ in lower.most_common(args.max_words)]
        Path(args.output).write_text(generate(ups,los))
        print(json.dumps({'upper_words':len(ups),'lower_words':len(los),'physical_wii_verified':False}))
    except (OSError,ValueError,KeyError,TypeError) as error:parser.exit(2,f'vu_aot_generate: {error}\n')

if __name__=='__main__':main()
