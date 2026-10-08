/* R1330 host regression tests for the EE game ELF loader. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "core/ee_elf_loader.h"
#include "core/ee/ee_core.h"
#include "core/bios_loader.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } else printf("ok:   %s\n", msg); } while (0)

static void wle32(uint8_t *p, uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static void wle16(uint8_t *p, uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}

static uint32_t build_synthetic_exec(uint8_t *buf)
{
    memset(buf,0,512);
    const uint32_t ph_off=52,ph_size=32,seg0_off=116,seg0_size=32,seg1_off=148,seg1_filesz=16;
    const uint32_t seg0_vaddr=0x00100000u,seg1_vaddr=0x00101000u,seg1_memsz=40;
    buf[0]=0x7f;buf[1]='E';buf[2]='L';buf[3]='F';buf[4]=1;buf[5]=1;buf[6]=1;
    wle16(buf+16,2);wle16(buf+18,8);wle32(buf+20,1);wle32(buf+24,seg0_vaddr+0x20);wle32(buf+28,ph_off);
    wle16(buf+40,52);wle16(buf+42,ph_size);wle16(buf+44,2);
    wle32(buf+ph_off+0,1);wle32(buf+ph_off+4,seg0_off);wle32(buf+ph_off+8,seg0_vaddr);wle32(buf+ph_off+12,seg0_vaddr);wle32(buf+ph_off+16,seg0_size);wle32(buf+ph_off+20,seg0_size);wle32(buf+ph_off+24,7);wle32(buf+ph_off+28,4);
    wle32(buf+ph_off+ph_size+0,1);wle32(buf+ph_off+ph_size+4,seg1_off);wle32(buf+ph_off+ph_size+8,seg1_vaddr);wle32(buf+ph_off+ph_size+12,seg1_vaddr);wle32(buf+ph_off+ph_size+16,seg1_filesz);wle32(buf+ph_off+ph_size+20,seg1_memsz);wle32(buf+ph_off+ph_size+24,7);wle32(buf+ph_off+ph_size+28,4);
    for(uint32_t i=0;i<seg0_size;i++)buf[seg0_off+i]=(uint8_t)(0xa0+i);
    for(uint32_t i=0;i<seg1_filesz;i++)buf[seg1_off+i]=(uint8_t)(0xb0+i);
    return seg1_off+seg1_filesz;
}

static void expect_reject(ee_state_t *st,uint8_t *image,uint32_t size,const char *msg)
{
    ee_elf_load_result_t r;const char *err=NULL;
    CHECK(ee_elf_load(st,image,size,&r,&err)==-1 && err!=NULL,msg);
}

int main(void)
{
    uint8_t image[512];uint32_t image_size=build_synthetic_exec(image);
    bios_image_t bios;memset(&bios,0,sizeof(bios));ee_core_init(&bios);ee_state_t *st=ee_core_get_state();
    st->tlb[0].entry_hi=0x00100000u;st->tlb[0].entry_lo0=((0x00100000u>>12)<<6)|7u;st->tlb[0].entry_lo1=((0x00101000u>>12)<<6)|7u;st->tlb[0].page_mask=0;
    ee_elf_load_result_t res;const char *err=NULL;
    CHECK(ee_elf_load(st,image,image_size,&res,&err)==0,"well-formed synthetic game ELF loads");
    CHECK(err==NULL,"success leaves error unset");
    CHECK(res.entry==0x00100020u,"entry is preserved");
    CHECK(res.load_start==0x00100000u && res.load_end==0x00101028u,"load range includes BSS");
    CHECK(ee_mem_read8(st,0x00100000u)==0xa0 && ee_mem_read8(st,0x0010100fu)==0xbf,"file bytes reach EE RAM");
    CHECK(ee_mem_read8(st,0x00101010u)==0 && ee_mem_read8(st,0x00101027u)==0,"BSS is zero-filled");
    CHECK(st->pc==BIOS_RESET_VECTOR,"loader does not change EE PC");

    {uint8_t bad[512];uint32_t n=build_synthetic_exec(bad);bad[0]='X';expect_reject(st,bad,n,"bad ELF magic rejected");}
    {uint8_t bad[512];uint32_t n=build_synthetic_exec(bad);wle16(bad+42,24);expect_reject(st,bad,n,"undersized program-header entries rejected");}
    {uint8_t bad[512];uint32_t n=build_synthetic_exec(bad);wle32(bad+28,n-8);expect_reject(st,bad,n,"program-header table outside image rejected");}
    {uint8_t bad[512];uint32_t n=build_synthetic_exec(bad);wle32(bad+52+16,33);wle32(bad+52+20,32);expect_reject(st,bad,n,"PT_LOAD filesz greater than memsz rejected");}
    {uint8_t bad[512];uint32_t n=build_synthetic_exec(bad);wle32(bad+52+4,0xfffffff0u);wle32(bad+52+16,32);wle32(bad+52+20,32);expect_reject(st,bad,n,"PT_LOAD file offset arithmetic overflow rejected");}
    {uint8_t bad[512];uint32_t n=build_synthetic_exec(bad);wle32(bad+52+8,0xffff0000u);wle32(bad+52+20,0x00200000u);expect_reject(st,bad,n,"PT_LOAD EE address overflow/out-of-RAM rejected");}

    printf("\n%d check(s) failed\n",failures);return failures?1:0;
}
