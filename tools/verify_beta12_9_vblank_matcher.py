#!/usr/bin/env python3
"""Test the exact EE VBlank signature function independently of giant EE core.

Extracts the function verbatim from source/core/ee/ee_core.c, compiles it
with narrowly defined CPU/RAM/MMIO fixtures, and asserts safe acceptance
and rejection. This does not emulate timing or prove Wii FPS gains.
"""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/core/ee/ee_core.c').read_text()
a=s.index('static uint32_t ee_vblank_skip_last_site;')
b=s.index('static uint64_t ee_vblank_skip_fast_hits,',a)
exact=s[a:b]
pre=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct{uint64_t ud0,ud1;} reg_t;
typedef struct{uint8_t *ram;uint32_t ram_size;reg_t gpr[32];} ee_state_t;
typedef struct{uint32_t stat;} intc_t;
static intc_t gs;
static intc_t *ee_intc_get_state(void){return &gs;}
static uint32_t ee_idle_rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put(uint8_t *mem,unsigned addr,uint32_t v){for(unsigned i=0;i<4;i++)mem[addr+i]=(uint8_t)(v>>(i*8));}
'''
post=r'''
int main(void){
 uint8_t ram[4096]={0};ee_state_t s={0};s.ram=ram;s.ram_size=sizeof(ram);
 /* LW $v1,0($s0); ANDI $v1,$v1,4; BEQZ $v1,-3; NOP */
 put(ram,0x100,0x8e030000);put(ram,0x104,0x30630004);
 put(ram,0x108,0x1060fffd);put(ram,0x10c,0);
 s.gpr[16].ud0=0x1000f000u;
 if(!ee_vblank_poll_signature(&s,0x100)||!ee_vblank_poll_signature(&s,0x104)||
    !ee_vblank_poll_signature(&s,0x108)||!ee_vblank_poll_signature(&s,0x10c))return 1;
 if(ee_vblank_skip_last_site!=0x100u)return 2;
 gs.stat=4;
 if(ee_vblank_poll_signature(&s,0x104))return 3;
 gs.stat=0;
 s.gpr[16].ud0=0x1000f004u;
 if(ee_vblank_poll_signature(&s,0x104))return 4;
 s.gpr[16].ud0=0x1000f000u;
 put(ram,0x10c,0xac000000);
 if(ee_vblank_poll_signature(&s,0x104))return 5;
 put(ram,0x10c,0);
 put(ram,0x108,0x1060fffe);
 if(ee_vblank_poll_signature(&s,0x104))return 6;
 put(ram,0x108,0x1060fffd);
 put(ram,0x100,0xae030000); /* SW, not LW */
 if(ee_vblank_poll_signature(&s,0x104))return 7;
 puts("PASS exact BETA12.9 hoisted-base matcher, IRQ guard, MMIO guard, side effects and branch guard");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='gekko2_matcher_') as t:
    p=Path(t)/'m.c';out=Path(t)/'m'
    p.write_text(pre+exact+post)
    subprocess.run(['gcc','-O2','-Wall',str(p),'-o',str(out)],check=True)
    subprocess.run([str(out)],check=True)
