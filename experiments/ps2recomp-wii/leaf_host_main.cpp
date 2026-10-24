/* GPL-3.0-or-later. Host driver for the deliberately limited leaf adapter. */
#include <cstdio>
#include <cstdlib>
#include "ps2_runtime.h"
extern "C" void recomp_leaf(R5900Context*);
int main(int argc,char **argv) {
 if(argc!=3&&argc!=4)return 2;
 R5900Context ctx{};
 if(argc==4){uint64_t seed=std::strtoull(argv[3],nullptr,0);for(unsigned r=1;r<32;r++)ctx.r[r].hi=seed^r;}
 ctx.r[4].lo=std::strtoull(argv[1],nullptr,0);
 ctx.r[5].lo=std::strtoull(argv[2],nullptr,0);
 ctx.r[29].lo=0x01fff000;ctx.r[31].lo=0x01fffff0;
 recomp_leaf(&ctx);
 std::printf("{\"pc\":\"%08x\",\"gpr\":[",ctx.pc);
 for(unsigned r=0;r<32;r++)std::printf("%s[\"%016llx\",\"%016llx\"]",r?",":"",(unsigned long long)ctx.r[r].lo,(unsigned long long)ctx.r[r].hi);
 std::printf("],\"hi\":[\"%016llx\",\"0000000000000000\"],\"lo\":[\"%016llx\",\"0000000000000000\"],\"sa\":%u}\n",(unsigned long long)ctx.hi,(unsigned long long)ctx.lo,ctx.sa);
 return ctx.pc==0x01fffff0&&!ctx.in_delay_slot?0:1;
}
