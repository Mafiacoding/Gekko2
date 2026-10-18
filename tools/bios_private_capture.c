/* Private, deterministic BIOS experiment. No guest patches or bundled assets.
 * Link with --wrap=time. Invocation/output policy is in investigate_bios.py. */
#include <time.h>
#include <stdint.h>
#include "core/runtime_profile.h"
uint32_t gekko2_profile_clock(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint32_t)((uint64_t)t.tv_sec*1000000000u+t.tv_nsec); }
time_t __wrap_time(time_t *out){time_t value=(time_t)1791806400;if(out)*out=value;return value;}
#define main bios_capture_main
#include "game_check_r1386.c"
#undef main
int main(int argc,char **argv)
{
 int result=bios_capture_main(argc,argv);
 const char *ram=getenv("GEKKO2_PRIVATE_RAM");
 if(ram){FILE *f=fopen(ram,"wb");if(!f)return 9;
  ee_state_t *e=ee_core_get_state();size_t bytes=fwrite(e->ram,1,e->ram_size,f);int failed=fclose(f);
  if(bytes!=e->ram_size||failed)return 9;
 }
 const char *snapshot=getenv("GEKKO2_PRIVATE_CHECKPOINT");
 if(snapshot&&checkpoint_save(snapshot))return 10;
 gekko2_profile profile;gekko2_profile_get(&profile);
 for(unsigned n=0;n<GP_COUNT;n++)printf("PROFILE_COST schema=1 scope=sampled_cpu category=%s ticks=%llu entries=%llu work=%llu unit=%s tb_hz=1000000000\n",gekko2_profile_name(n),(unsigned long long)profile.ticks[n],(unsigned long long)profile.entries[n],(unsigned long long)profile.work[n],gekko2_profile_unit(n));
 for(unsigned n=0;n<GIF_GOURAUD_STAT_COUNT;n++)printf("GS_TRI_STAT %u=%llu\n",n,(unsigned long long)gif_get_gouraud_stat(n));
 return result;
}
