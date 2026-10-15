#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "core/hw/arm_loader.h"
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
 assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>52&&n<1048576);rewind(f);
 unsigned char *e=malloc(n);assert(e&&fread(e,1,n,f)==(unsigned long)n);fclose(f);
 arm_load_plan_t p;assert(arm_loader_validate(e,n,0x13700000,0x80000,&p));
 assert(p.entry==0x13700000&&p.stack_size==0x3000&&p.count==2);
 free(e);puts("PASS shipped new ARM ELF validates inside MLOAD region, including private staging, metadata and thread stack");return 0;
}
