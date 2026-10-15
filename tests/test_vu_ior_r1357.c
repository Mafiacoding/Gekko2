/* Literal opcode from Tekken VU1 PC 0x698/0x558. No enum-built oracle.
 * PCSX2 VUops.cpp LowerOP_OPCODE slots 0x34=IAND, 0x35=IOR.
 */
#include "core/hw/vu.h"
#include "core/ee/ee_core.h"
#include <stdio.h>
#include <string.h>
static void word(unsigned char *p,unsigned w){p[0]=w;p[1]=w>>8;p[2]=w>>16;p[3]=w>>24;}
static int errors;
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);errors++;}}while(0)
static void pair(unsigned w){vu1_micro_write32(0,w);vu1_micro_write32(4,0x400002ff);vu1_micro_write32(8,0x8000033c);vu1_micro_write32(12,0x2ff);}
int main(void){
 vu1_init();vu1_state_t *v=vu1_get_state();v->vi[1]=0x12340050;v->vi[2]=0x56780a03;
 pair(0x80020875);vu1_exec_micro(0);CHECK(v->vi[1]==0x0a53);CHECK(v->unimplemented_opcodes_seen==0);
 vu1_init();v=vu1_get_state();v->vi[1]=0x50;v->vi[2]=0xa03;
 pair(0x80020835);vu1_exec_micro(0);CHECK(v->vi[0]==0);CHECK(v->vi[1]==0x50);CHECK(v->unimplemented_opcodes_seen==0);
 vu1_init();v=vu1_get_state();v->vi[1]=0x50;v->vi[2]=0xa03;
 pair(0x80020873);vu1_exec_micro(0);CHECK(v->vi[1]==0x50);CHECK(v->unimplemented_opcodes_seen==1);
 ee_state_t ee;memset(&ee,0,sizeof ee);ee.cop2_ctrl[1]=0x12340050;ee.cop2_ctrl[2]=0x56780a03;
 vu0_micro_write32(&ee,0,0x80020875);vu0_micro_write32(&ee,4,0x400002ff);vu0_micro_write32(&ee,8,0x8000033c);vu0_micro_write32(&ee,12,0x2ff);
 vu0_exec_micro(&ee,0);CHECK(ee.cop2_ctrl[1]==0xa53);CHECK(ee.vu0_unimplemented_opcodes_seen==0);
 printf("%s literal Tekken IOR, 16-bit result, VI0, reserved opcode, VU0/VU1\n",errors?"FAIL":"PASS");return errors!=0;
}
