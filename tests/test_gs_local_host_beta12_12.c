/* GS local-to-host FIFO regression, native host endian independent. */
#include <stdio.h>
#include <string.h>
#include "hw/gs_mem.c"
static int bad;
#define CHECK(expr) do{if(!(expr)){fprintf(stderr,"FAIL at %d: %s\n",__LINE__,#expr);bad++;}}while(0)
static gs_local_copy_desc_t test_desc(unsigned psm,unsigned w,unsigned h){
 gs_local_copy_desc_t d={0};d.sbp=0;d.sbw=128;d.spsm=psm;d.rrw=w;d.rrh=h;return d;
}
int main(void){
 uint8_t bytes[128];gs_mem_init();
 gs_local_copy_desc_t d=test_desc(0,3,1);
 gs_mem_write_psmct32(0,128,0,0,0x11223344u);
 gs_mem_write_psmct32(0,128,1,0,0x55aabbccu);
 gs_mem_write_psmct32(0,128,2,0,0xdeadbeefu);
 CHECK(gs_mem_local_fifo_total_qwords(&d)==1);
 memset(bytes,0xaa,sizeof bytes);
 CHECK(gs_mem_read_local_qwords(&d,0,bytes,1)==1);
 static const uint8_t expect[]={0x44,0x33,0x22,0x11,0xcc,0xbb,0xaa,0x55,0xef,0xbe,0xad,0xde};
 CHECK(memcmp(bytes,expect,sizeof expect)==0);
 CHECK(bytes[12]==0 && bytes[15]==0);
 CHECK(gs_mem_read_local_qwords(&d,1,bytes,1)==0);
 d=test_desc(1,2,1);
 gs_mem_write_psmct32(0,128,0,0,0x89123456u);
 gs_mem_write_psmct32(0,128,1,0,0xabcdef01u);
 CHECK(gs_mem_read_local_qwords(&d,0,bytes,1)==1);
 CHECK(bytes[0]==0x56&&bytes[1]==0x34&&bytes[2]==0x12&&
       bytes[3]==0x01&&bytes[4]==0xef&&bytes[5]==0xcd&&bytes[6]==0);
 d=test_desc(2,2,1);
 gs_mem_write_psmct16(0,128,0,0,0x1234);
 gs_mem_write_psmct16(0,128,1,0,0xabcd);
 CHECK(gs_mem_read_local_qwords(&d,0,bytes,1)==1);
 CHECK(bytes[0]==0x34&&bytes[1]==0x12&&bytes[2]==0xcd&&bytes[3]==0xab);
 d=test_desc(0x14,5,1);
 for(unsigned i=0;i<5;i++)gs_mem_write_index(0,128,i,0,0x14,i+1);
 CHECK(gs_mem_read_local_qwords(&d,0,bytes,1)==1);
 CHECK(bytes[0]==0x21&&bytes[1]==0x43&&bytes[2]==0x05&&bytes[3]==0);
 d=test_desc(0,5,1);
 CHECK(gs_mem_local_fifo_total_qwords(&d)==2);
 CHECK(gs_mem_read_local_qwords(&d,0,bytes,2)==2);
 CHECK(gs_mem_read_local_qwords(&d,1,bytes,2)==1);
 d=test_desc(3,1,1);
 CHECK(gs_mem_local_fifo_total_qwords(&d)==0);
 CHECK(gs_mem_read_local_qwords(&d,0,bytes,1)==0);
 d=test_desc(0,2048,2048);d.sbp=16380;
 CHECK(gs_mem_local_fifo_total_qwords(&d)==0);
 puts(bad?"FAIL: GS local-to-host":"PASS: GS local-to-host");
 return !!bad;
}
