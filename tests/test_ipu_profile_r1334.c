#include "core/hw/ipu.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
 ipu_profile_t s;uint32_t ctrl,busy;unsigned char input[160]={0};
 ipu_init();ipu_process_quadwords(4,input,10);
 assert(ipu_mmio_read32(0x10002010,&ctrl)&&ctrl==8);
 assert(ipu_mmio_write32(0x10002000,0x10000023));
 assert(ipu_mmio_read32(0x10002004,&busy)&&busy==0);
 ipu_get_profile(&s);assert(s.commands[1]==1&&s.unimplemented_commands==1&&s.fifo_count==8);
 assert(s.input_qwc==10&&s.accepted_qwc==8&&s.discarded_qwc==10&&s.last_command==0x10000023);
 assert(ipu_mmio_write32(0x10002000,0));
 assert(ipu_mmio_read32(0x10002010,&ctrl)&&ctrl==0);
 ipu_get_profile(&s);assert(s.commands[0]==1&&s.commands[1]==1&&!s.fifo_count);
 /* A guest reset preserves observations; the next cold boot clears them. */
 assert(ipu_mmio_write32(0x10002010,0x40000000));
 ipu_get_profile(&s);assert(s.commands[1]==1);
 ipu_init();ipu_get_profile(&s);assert(!s.commands[0]&&!s.commands[1]&&!s.input_qwc&&!s.unimplemented_commands&&!s.last_command);
 puts("PASS IPU diagnostics report missing decode without changing existing register behavior; cold boot clears observations");
 return 0;
}
