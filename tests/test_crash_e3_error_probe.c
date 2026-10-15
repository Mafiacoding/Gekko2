#include "core/hw/guest_error_probe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void write32(unsigned char *p,unsigned v)
{
    p[0]=v&255; p[1]=(v>>8)&255;p[2]=(v>>16)&255;p[3]=(v>>24)&255;
}
int main(void)
{
    ee_state_t *st=calloc(1,sizeof(*st));
    gekko2_guest_error_t e;
    assert(st);
    st->ram_size=0x02000000u;
    st->ram=calloc(1,st->ram_size);
    assert(st->ram);
    st->pc=0x00103d70u;
    st->gpr[29].ud0=0x01fff000u;
    assert(!gekko2_guest_error_probe(st,&e));
    memcpy(st->ram+0x00468800u,"nu2.err",8);
    write32(st->ram+0x00103d70u,0x1000ffffu);
    assert(!gekko2_guest_error_probe(st,&e));
    st->gpr[29].ud0=0x00180000u;
    memcpy(st->ram+0x00181000u,"NuError - main.cpp(42) : ",25);
    memcpy(st->ram+0x00180000u,"unable to create font fonts\\ariel32",35);
    write32(st->ram+0x00181120u,0x00165438u);
    assert(gekko2_guest_error_probe(st,&e));
    assert(e.pc==0x00103d70u && e.sp==0x00180000u && e.caller==0x00165438u);
    assert(strstr(e.heading,"NuError")!=NULL);
    assert(strstr(e.detail,"unable to create font")!=NULL);
    /* The guest may include backslashes in paths, but not a forged log newline. */
    st->ram[0x00180000u+6u]='\n';
    assert(gekko2_guest_error_probe(st,&e));
    assert(strchr(e.detail,'\n')==NULL);
    st->pc=0x00103d74u;
    assert(gekko2_guest_error_probe(st,&e));
    st->pc=0x00103d78u;
    assert(!gekko2_guest_error_probe(st,&e));
    st->pc=0x00103d70u;
    st->ram[0x00468800u]='X';
    assert(!gekko2_guest_error_probe(st,&e));
    free(st->ram);free(st);
    puts("test_crash_e3_error_probe: PASS");
    return 0;
}
