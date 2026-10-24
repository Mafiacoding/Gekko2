/* GPL-3.0-or-later. Host-only isolated EE function oracle for recompiler tests.
 * Loads a caller-supplied ELF privately. This is not the game boot flow and
 * does not prove syscall, timing, device or full-game compatibility. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include "core/system.h"
#include "core/ee_elf_loader.h"
#include "core/recompiler/optimization.h"
#define RETURN_PC 0x01fffff0u
static int number(const char*s,uint64_t*v){char*end;errno=0;if(!*s||isspace((unsigned char)*s)||*s=='-'||*s=='+')return 0;*v=strtoull(s,&end,0);return !errno&&!*end;}
int main(int argc,char**argv){
 uint64_t entry,budget,a0=0,a1=0,upper=0;
 if(argc<4||argc>7||!number(argv[2],&entry)||entry>UINT32_MAX||(entry&3)||
    !number(argv[3],&budget)||!budget||budget>1000000||
    (argc>4&&!number(argv[4],&a0))||(argc>5&&!number(argv[5],&a1))||(argc>6&&!number(argv[6],&upper))){
  fprintf(stderr,"usage: %s ELF ENTRY STEP-LIMIT [A0 [A1 [UPPER-SEED]]]\n",argv[0]);return 2;
 }
 FILE*f=fopen(argv[1],"rb");if(!f){perror("ELF");return 3;}
 if(fseek(f,0,SEEK_END)){fclose(f);return 3;}long n=ftell(f);
 if(n<52||n>32*1024*1024||fseek(f,0,SEEK_SET)){fclose(f);return 3;}
 uint8_t*image=malloc((size_t)n);if(!image){fclose(f);return 3;}
 if(fread(image,1,(size_t)n,f)!=(size_t)n){free(image);fclose(f);return 3;}fclose(f);
 gekko2_optimization_mask=0; /* Explicit interpreter oracle, no native claim. */
 bios_image_t bios={0};if(system_init(&bios,&bios)){free(image);return 3;}
 ee_state_t*st=ee_core_get_state();
 /* Explicit flat mapping for an isolated leaf-function test: two 16-MiB
  * pages, valid/dirty/global. Real BIOS/game mappings remain untouched. */
 st->cop0[12]=0;st->cop0[13]=0;
 st->tlb[0].page_mask=0x01ffe000;st->tlb[0].entry_hi=0;
 st->tlb[0].entry_lo0=7;st->tlb[0].entry_lo1=0x40007;
 ee_fastmem_invalidate();
 ee_elf_load_result_t loaded;const char*error=NULL;
 if(ee_elf_load(st,image,(uint32_t)n,&loaded,&error)){free(image);fprintf(stderr,"ELF load: %s\n",error?error:"failed");return 3;}free(image);
 if(entry==RETURN_PC||entry<loaded.load_start||entry>=loaded.load_end){fprintf(stderr,"entry outside loaded image\n");return 2;}
 /* Deliberate isolated test-call context, not a replacement BIOS startup. */
 st->pc=(uint32_t)entry;st->next_pc=st->pc+4;st->branch_pending=0;
 if(argc>6)for(unsigned r=1;r<32;r++)st->gpr[r].ud1=upper^r;
 st->gpr[4].ud0=a0;st->gpr[5].ud0=a1;st->gpr[29].ud0=0x01fff000;st->gpr[31].ud0=RETURN_PC;
 uint64_t before=st->instructions_executed;unsigned steps=0;
 while(steps<budget&&st->pc!=RETURN_PC&&!st->halted){steps++;if(ee_core_step()||(st->cop0[13]&0x7cu))break;}
 int completed=st->pc==RETURN_PC&&!st->halted;
 printf("{\"kind\":\"isolated-ee-function\",\"host_PPC_execution\":false,\"game_boot\":false,\"completed\":%s,\"steps\":%u,\"retired\":%"PRIu64",\"pc\":\"%08"PRIx32"\",\"gpr\":[",completed?"true":"false",steps,st->instructions_executed-before,st->pc);
 for(unsigned r=0;r<32;r++)printf("%s[\"%016"PRIx64"\",\"%016"PRIx64"\"]",r?",":"",st->gpr[r].ud0,st->gpr[r].ud1);
 printf("],\"hi\":[\"%016"PRIx64"\",\"%016"PRIx64"\"],\"lo\":[\"%016"PRIx64"\",\"%016"PRIx64"\"],\"sa\":%u}\n",st->hi.ud0,st->hi.ud1,st->lo.ud0,st->lo.ud1,st->sa_reg);
 return completed?0:1;
}
