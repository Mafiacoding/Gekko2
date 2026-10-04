#include "core/hw/gs_mem.h"
#include <stdio.h>
static int calls,fail;
static int resolve(void *p,uint8_t *v,uint32_t size){(void)p;(void)size;calls++;if(fail)return 0;v[0]=99;return 1;}
#define C(c) do{if(!(c)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(void){uint64_t h;gs_mem_init();gs_mem_write_psmct32(100000,64,0,0,0x12345678);
 C(gs_mem_gpu_bind(resolve,0));C(gs_mem_gpu_protect_range(0,8192));C(gs_mem_gpu_mark_pending());
 C(gs_mem_read_psmct32(100000,64,0,0)==0x12345678);C(calls==0&&gs_mem_gpu_pending());
 C(gs_mem_hash_range(400000,64,&h));C(calls==0&&gs_mem_gpu_pending());
 C(gs_mem_read_psmct32(0,64,0,0)==99);C(calls==1&&!gs_mem_gpu_pending());
 C(gs_mem_gpu_protect_range(0,8192));C(gs_mem_gpu_mark_pending());gs_mem_write_psmct32(100000,64,0,0,7);C(calls==2&&!gs_mem_gpu_pending());
 C(gs_mem_gpu_protect_range(0,8192));C(gs_mem_gpu_mark_pending());C(gs_mem_get()!=0);C(calls==3);
 C(gs_mem_gpu_bind(resolve,0));C(gs_mem_gpu_mark_pending());C(gs_mem_read_psmct32(100000,64,0,0)==7);C(calls==4);
 C(gs_mem_gpu_protect_range(0,8192));C(gs_mem_gpu_mark_pending());fail=1;C(gs_mem_read_psmct32(100000,64,0,0)==7);C(calls==4);C(gs_mem_read_psmct32(0,64,0,0)==0);C(calls==5&&gs_mem_gpu_pending());C(gs_mem_get()==0);fail=0;C(gs_mem_sync());
 C(!gs_mem_gpu_protect_range(8192,0));C(!gs_mem_hash_range(GS_MEM_SIZE-1,2,&h));puts("PASS guarded nonalias reads, writes/raw access resolve, new binding resets guard and failed owner stays protected");return 0;}
