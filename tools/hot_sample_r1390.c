#include "core/checkpoint.h"
#include "core/iso_loader.h"
#include "core/bios_loader.h"
#include "core/system.h"
#include "core/ee/ee_core.h"
#include "core/iop/iop_core.h"
#include "core/hw/iop_cdvd.h"
#include "core/hw/iop_cdrom_legacy.h"
#include "core/recompiler/optimization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
uint32_t ee_pcprof[1u<<23];
uint32_t iop_pcprof[1u<<20];
typedef struct{uint32_t a;uint64_t n;}ent;
static int cmp(const void*a,const void*b){uint64_t x=((const ent*)a)->n,y=((const ent*)b)->n;return x<y?1:x>y?-1:0;}
int main(int argc,char **argv){
 extern uint32_t gekko2_optimization_mask;gekko2_optimization_mask|=GEKKO2_OPT_BIT(GEKKO2_OPT_IOP_IDLE_BATCH)|GEKKO2_OPT_BIT(GEKKO2_OPT_EVENT_SCHEDULER);
 bios_image_t bios;if(bios_load(argv[1],&bios)||system_init(&bios,&bios))return 3;
 int a=iop_cdvd_mount_iso(argv[2]),b=iop_cdrom_legacy_mount_iso(argv[2]);if(a&&b)return 4;if(!a)iop_cdvd_set_disc_present(0x12);
 {const char *rs=getenv("GEKKO2_RESUME");if(rs&&checkpoint_load(rs,&bios,&bios,argv[2]))return 8;}
 unsigned from=strtoul(argv[3],0,0),to=strtoul(argv[4],0,0);
 for(unsigned n=0;n<to;n++){if(n==from){memset(ee_pcprof,0,sizeof ee_pcprof);memset(iop_pcprof,0,sizeof iop_pcprof);}system_run_interleaved(1000000);}
 uint64_t total=0;for(unsigned i=0;i<(1u<<23);i++)total+=ee_pcprof[i];
 {extern uint64_t ee_idle_skip_stats[3];printf("SKIP calls=%llu slots=%llu\n",(unsigned long long)ee_idle_skip_stats[0],(unsigned long long)ee_idle_skip_stats[1]);}
 {extern uint64_t iop_spin_skip_stats[4];printf("IOPSKIP %llu %llu\n",(unsigned long long)iop_spin_skip_stats[0],(unsigned long long)iop_spin_skip_stats[1]);}
 printf("TOTAL %llu\n",(unsigned long long)total);
 /* 64-byte buckets */
 static ent bk[1<<17];unsigned nb=0;
 for(unsigned i=0;i<(1u<<23);i+=16){uint64_t s=0;for(unsigned k=0;k<16;k++)s+=ee_pcprof[i+k];if(s){bk[nb].a=i*4;bk[nb].n=s;nb++;}}
 qsort(bk,nb,sizeof bk[0],cmp);
 for(unsigned i=0;i<60&&i<nb;i++)printf("BLK %08x %llu %.2f%%\n",bk[i].a,(unsigned long long)bk[i].n,100.0*bk[i].n/total);
 {static ent ib[1<<16];unsigned ni=0;uint64_t it=0;for(unsigned i=0;i<(1u<<20);i+=16){uint64_t q=0;for(unsigned k=0;k<16;k++)q+=iop_pcprof[i+k];it+=q;if(q){ib[ni].a=i*4;ib[ni].n=q;ni++;}}
 qsort(ib,ni,sizeof ib[0],cmp);printf("IOPTOTAL %llu\n",(unsigned long long)it);for(unsigned i=0;i<12&&i<ni;i++)printf("IBLK %08x %llu %.2f%%\n",ib[i].a,(unsigned long long)ib[i].n,100.0*ib[i].n/it);}
 {static ent ip[1<<18];unsigned nn=0;for(unsigned i=0;i<(1u<<20);i++)if(iop_pcprof[i]){ip[nn].a=i*4;ip[nn].n=iop_pcprof[i];nn++;}qsort(ip,nn,sizeof ip[0],cmp);for(unsigned i=0;i<70&&i<nn;i++)printf("IPC %08x %llu\n",ip[i].a,(unsigned long long)ip[i].n);}
 {FILE*o=fopen("/tmp/iop_ram.bin","wb");fwrite(iop_core_get_state()->ram,1,0x200000,o);fclose(o);}
 return 0;}
