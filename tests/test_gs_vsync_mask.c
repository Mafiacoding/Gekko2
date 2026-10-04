/* MMIO-visible CSR and IMR positions must not share a bit index. */
#include <stdio.h>
#include "core/ee/ee_core.c"
int main(void){
 bios_image_t b={0}; if(system_init(&b,&b))return 2;
 ee_state_t*s=ee_core_get_state(); gs_state_t*g=gs_get_state();
 ee_intc_state_t*ic=ee_intc_get_state(); unsigned fail=0;
 s->instructions_executed=0;ic->stat=0;ee_check_gs_vsync(s);
 if(!(g->csr&8)||(ic->stat&1)){puts("FAIL: reset masks GS interrupt while CSR records VSYNC");fail++;}
 gs_mmio_write64(0x12001010,0x1f00&~0x800);ic->stat=0;ee_check_gs_vsync(s);
 if(!(ic->stat&1)){puts("FAIL: hardware VSMSK bit11 unmasks GS interrupt");fail++;}
 gs_mmio_write64(0x12001010,0x800);ic->stat=0;ee_check_gs_vsync(s);
 if(ic->stat&1){puts("FAIL: low bit3 clear must not unmask hardware bit11");fail++;}
 gs_mmio_write64(0x12000020,1);ee_core_display_clock_load(EE_CYCLES_PER_FRAME_NTSC);ee_check_vblank(s);
 uint64_t first=g->csr&(1ull<<13);ee_core_display_clock_load(ee_core_display_clock_save()+EE_CYCLES_PER_FRAME_NTSC);ee_check_vblank(s);
 if((g->csr&(1ull<<13))==first){puts("FAIL: interlaced FIELD does not alternate");fail++;}
 g->csr|=1ull<<13;gs_mmio_write64(0x12001000,~0ull);
 if(!(g->csr&(1ull<<13))){puts("FAIL: CSR write destroys read-only FIELD");fail++;}
 gs_mmio_write64(0x12000020,0);ee_core_display_clock_load(ee_core_display_clock_save()+EE_CYCLES_PER_FRAME_NTSC);ee_check_vblank(s);
 if(g->csr&(1ull<<13)){puts("FAIL: progressive FIELD alternates");fail++;}
 /* GS VSYNC must follow hardware ticks, not instruction retirement. */
 gs_mmio_write64(0x12001010,0x1f00&~0x800);
 s->instructions_executed=0;ee_core_display_clock_load(1);s->cop0[9]=0;g->csr&=~8ull;ic->stat=0;ee_check_gs_vsync(s);
 if((g->csr&8)||(ic->stat&1)){puts("FAIL: frozen zero instruction count spuriously emits VSYNC");fail++;}
 s->instructions_executed=17;ee_core_display_clock_load(EE_CYCLES_PER_FRAME_NTSC);ee_check_gs_vsync(s);
 if(!(g->csr&8)||!(ic->stat&1)){puts("FAIL: hardware frame boundary fails with parked instruction count");fail++;}
 ee_core_display_clock_load(EE_CYCLES_PER_FRAME_NTSC-1);g->csr&=~8ull;ic->stat=0;ee_core_park_tick(s);
 if(!(g->csr&8)||!(ic->stat&1)){puts("FAIL: parked EE does not emit actual VSYNC edge");fail++;}
 g->csr&=~8ull;ic->stat=0;ee_core_park_tick(s);
 if((g->csr&8)||(ic->stat&1)){puts("FAIL: parked EE repeats VSYNC after frame edge");fail++;}
 printf("GS VSYNC mask: %u failures\n",fail);return fail?1:0;
}
