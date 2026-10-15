/* GPL-3.0-or-later. Host-only observed-entry profiler; private output. */
#include <stdio.h>
#include <stdlib.h>
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/recompiler/optimization.h"
#include "core/ee/ee_core.h"
#include "core/hw/vu.h"
#include "core/iop/iop_core.h"
#include <string.h>
#include "core/checkpoint.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/iop_cdrom_legacy.h"
typedef struct { uint32_t pc, words[32]; unsigned count; uint64_t hits; } record;
static record entries[16384];
static record iop_entries[8192];
int main(int argc, char **argv)
{
    if(argc!=4&&argc!=5){fprintf(stderr,"usage: ee_aot_profile BIOS OUTPUT_JSON SLICES\n");return 2;}
    char *end;unsigned long long slices=strtoull(argv[3],&end,10);
    if(!slices||*end){fprintf(stderr,"invalid bounded slice count\n");return 2;}
    if(getenv("GEKKO2_OPTIONS"))gekko2_optimization_mask=strtoul(getenv("GEKKO2_OPTIONS"),NULL,16);
    bios_image_t bios={0};
    if(bios_load(argv[1],&bios)||system_init(&bios,&bios))return 1;
    if(argc==5){int a=iop_cdvd_mount_iso(argv[4]),b=iop_cdrom_legacy_mount_iso(argv[4]);if(a&&b)return 3;if(!a)iop_cdvd_set_disc_present(0x12);}
    const char *resume=getenv("GEKKO2_RESUME");if(resume&&checkpoint_load(resume,&bios,&bios,argc==5?argv[4]:NULL))return 4;
    unsigned stride=getenv("GEKKO2_PROFILE_STRIDE")?strtoul(getenv("GEKKO2_PROFILE_STRIDE"),NULL,0):1u;
    if(!stride||stride>1024)return 2;
    gekko2_optimization_mask &= ~(GEKKO2_OPT_BIT(GEKKO2_OPT_EE_BLOCKS)|GEKKO2_OPT_BIT(GEKKO2_OPT_VU_JIT)|GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_JIT));
    for(unsigned long long i=0;i<slices;i+=stride) {
        ee_state_t *st=ee_core_get_state();
        if(!st->idle&&!st->halted&&!st->branch_pending) {
            unsigned slot=((st->pc>>2)^(st->pc>>12))&16383u;
            record *r=&entries[slot];
            if(!r->hits||r->pc==st->pc) {
                if(!r->hits) {
                    r->count=ee_core_block_words(st,st->pc,r->words,32u);
                    r->pc=st->pc;
                }
                if(r->count)r->hits++;
            }
        }
        iop_state_t *io=iop_core_get_state();
        uint32_t physical=io->pc&0x1fffffffu;
        const uint8_t *code=NULL;
        if(!io->idle&&!io->halted&&!(io->pc&3u)){
            if(physical<=io->ram_size&&io->ram_size-physical>=4u)code=io->ram+physical;
            else if(physical>=0x1fc00000u&&physical-0x1fc00000u<=bios.size&&bios.size-(physical-0x1fc00000u)>=4u)code=bios.data+physical-0x1fc00000u;
        }
        if(code){record *r=&iop_entries[((io->pc>>2)^(io->pc>>12))&8191u];if(!r->hits||r->pc==io->pc){r->pc=io->pc;r->count=1;r->words[0]=(uint32_t)code[0]|((uint32_t)code[1]<<8)|((uint32_t)code[2]<<16)|((uint32_t)code[3]<<24);r->hits++;}}
        if(system_run_interleaved(slices-i<stride?slices-i:stride))break;
    }
    FILE *out=fopen(argv[2],"w");if(!out)return 1;
    fputs("{\"scope\":\"host-observed-entry-sampling\",\"blocks\":[",out);
    unsigned emitted=0;
    for(unsigned i=0;i<16384u;i++)if(entries[i].hits) {
        record *r=&entries[i];
        fprintf(out,"%s{\"pc\":\"0x%08x\",\"hits\":%llu,\"words\":[",emitted++?",":"",r->pc,(unsigned long long)r->hits);
        for(unsigned j=0;j<r->count;j++)fprintf(out,"%s\"0x%08x\"",j?",":"",r->words[j]);
        fputs("]}",out);
    }
    fputs("],\"iop_blocks\":[",out);emitted=0;
    for(unsigned i=0;i<8192u;i++)if(iop_entries[i].hits){record *r=&iop_entries[i];fprintf(out,"%s{\"pc\":\"0x%08x\",\"hits\":%llu,\"words\":[\"0x%08x\"]}",emitted++?",":"",r->pc,(unsigned long long)r->hits,r->words[0]);}
    fputs("]}\n",out);
    int failed=ferror(out);if(fclose(out))failed=1;
    const uint8_t *micro[2]={ee_core_get_state()->vu0_micro,vu1_get_state()->micro};
    const size_t sizes[2]={sizeof(ee_core_get_state()->vu0_micro),VU1_MICRO_SIZE};
    for(unsigned unit=0;unit<2;unit++){
        char *path=malloc(strlen(argv[2])+16u);if(!path)return 1;
        sprintf(path,"%s.vu%u.bin",argv[2],unit);
        FILE *m=fopen(path,"wb");free(path);if(!m)return 1;
        if(fwrite(micro[unit],1,sizes[unit],m)!=sizes[unit])failed=1;
        if(fclose(m))failed=1;
    }
    return failed?1:0;
}
