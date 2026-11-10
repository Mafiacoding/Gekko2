#include "core/hw/ipu.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
 ipu_profile_t s;uint32_t ctrl,busy;unsigned char input[160]={0};
 ipu_init();ipu_process_quadwords(4,input,10);
 assert(ipu_mmio_read32(0x10002010,&ctrl)&&ctrl==8);
 /* IDEC has been implemented since R1337. Use a genuinely unsupported
  * opcode instead of requiring the old unsupported-MPEG acknowledgement. */
 assert(ipu_mmio_write32(0x10002000,0xf0000023));
 assert(ipu_mmio_read32(0x10002004,&busy)&&busy==0);
 ipu_get_profile(&s);assert(s.commands[15]==1&&s.unimplemented_commands==1&&s.fifo_count==8);
 assert(s.input_qwc==10&&s.accepted_qwc==8&&s.discarded_qwc==2&&s.last_command==0xf0000023);
 assert(ipu_mmio_write32(0x10002000,0));
 assert(ipu_mmio_read32(0x10002010,&ctrl)&&ctrl==0);
 ipu_get_profile(&s);assert(s.commands[0]==1&&s.commands[15]==1&&!s.fifo_count);
 /* A guest reset preserves observations; the next cold boot clears them. */
 assert(ipu_mmio_write32(0x10002010,0x40000000));
 ipu_get_profile(&s);assert(s.commands[15]==1);
 ipu_init();ipu_get_profile(&s);assert(!s.commands[0]&&!s.commands[15]&&!s.input_qwc&&!s.unimplemented_commands&&!s.last_command);
 puts("PASS IPU diagnostics distinguish retained input, overflow and unsupported opcode; cold boot clears observations");
 return 0;
}
