#!/usr/bin/env python3
"""Generate exact-PC IOP instructions against the existing precise pipeline."""
import argparse
import json
from pathlib import Path

def supported(w):
    op,rs,rt,fn=w>>26,w>>21&31,w>>16&31,w&63
    if op==0:return fn in (0,2,3,4,6,7,16,17,18,19,24,25,26,27,32,33,34,35,36,37,38,39,42,43,48)
    if op==1:return rt in (0,1,16,17)
    if op==16:return rs in (0,4) or rs==16 and fn==16
    return op in (4,5,6,7,8,9,10,11,12,13,14,15,32,33,34,35,36,37,38,40,41,42,43,46,47)

def generate(entries):
    out=['/* GPL-3.0-or-later. Exact-word IOP static bodies with live tick/pipeline admission. */','#define GPR(x) st->gpr[x]','#define BRANCH_TO(target) do { st->next_pc=(target); } while(0)','#define LINK(reg) do { GPR(reg)=this_pc+8u; } while(0)']
    rows=[]
    for i,(pc,w) in enumerate(sorted(entries)):
        if pc&3 or not 0<=pc<=0xffffffff or not 0<=w<=0xffffffff or not supported(w):raise ValueError('invalid/unsupported IOP entry')
        out += [f'static int iop_aot_body_{i}(iop_state_t *st) {{',f'const uint32_t this_pc=UINT32_C(0x{pc:08x}),instr=UINT32_C(0x{w:08x});',
                'const uint32_t op=instr>>26,rs=(instr>>21)&31u,rt=(instr>>16)&31u,rd=(instr>>11)&31u,sa=(instr>>6)&31u,funct=instr&63u;',
                'const uint32_t rs32=GPR(rs),rt32=GPR(rt),uimm=instr&65535u,fallthrough_pc=st->pc;const int32_t imm=(int16_t)instr;',
                '(void)rs32;(void)rt32;(void)uimm;(void)imm;(void)rd;(void)sa;(void)funct;(void)fallthrough_pc;',
                '#include "core/iop/iop_decoded_switch.inc"','return 0;','}']
        rows.append(f'{{UINT32_C(0x{pc:08x}),UINT32_C(0x{w:08x}),iop_aot_body_{i}}}')
    if len({pc for pc,_ in entries})!=len(entries):raise ValueError('duplicate IOP PC')
    out += ['#undef GPR','#undef BRANCH_TO','#undef LINK',
            'typedef struct {uint32_t pc,word;int (*fn)(iop_state_t *);} iop_aot_entry;',
            'static const iop_aot_entry iop_aot_entries[]={'+','.join(rows or ['{0u,0u,NULL}'])+'};',
            'static uint64_t iop_aot_executed;uint64_t iop_aot_executed_count(void){return iop_aot_executed;}',
            'static unsigned iop_aot_try_execute(iop_state_t *st,unsigned budget){',
            '#ifdef GEKKO2_IOP_AOT',
            'if(!budget||st!=&g_iop||st->halted||st->idle||!gekko2_opt_enabled(GEKKO2_OPT_IOP_JIT))return 0;',
            f'unsigned lo=0,hi={len(rows)}u;while(lo<hi){{unsigned mid=lo+(hi-lo)/2;const iop_aot_entry *e=&iop_aot_entries[mid];if(st->pc<e->pc)hi=mid;else if(st->pc>e->pc)lo=mid+1;else{{',
            'int admitted=iop_core_block_prepare(st,e->pc,e->word,UINT32_MAX);if(!admitted)return 0;if(admitted==2)return 1;',
            'int failed=e->fn(st);if(!failed){iop_core_block_retire(st,e->pc);iop_aot_executed++;}return 1;',
            '}}', '#else','(void)st;(void)budget;','#endif','return 0;','}','']
    return '\n'.join(out)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--profile',required=True);parser.add_argument('--max-entries',type=int,default=256);parser.add_argument('--output',required=True);a=parser.parse_args()
    try:
        if not 1<=a.max_entries<=1024:raise ValueError('max-entries must be 1..1024')
        records=json.loads(Path(a.profile).read_text())['iop_blocks'];entries=[]
        for r in sorted(records,key=lambda r:-int(r['hits'])):
            pc=int(r['pc'],0) if isinstance(r['pc'],str) else int(r['pc']);w=int(r['words'][0],0) if isinstance(r['words'][0],str) else int(r['words'][0])
            if supported(w):entries.append((pc,w))
            if len(entries)==a.max_entries:break
        Path(a.output).write_text(generate(entries));print(json.dumps({'iop_entries':len(entries),'physical_wii_verified':False}))
    except (OSError,ValueError,KeyError,TypeError) as error:parser.exit(2,f'iop_aot_generate: {error}\n')

if __name__=='__main__':main()
