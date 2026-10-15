/* GPL-3.0-or-later. Locally generated, exact-word VU specializations. */
static int vu_aot_has_upper(uint32_t word) {
switch(word) {
default:return 0;
}
}
static int vu_aot_try_upper(uint32_t vf[32][4],uint32_t *vi,uint32_t acc[4],uint32_t word) {
if(!gekko2_opt_enabled(GEKKO2_OPT_VU_JIT))return 0;
word&=0x01ffffffu;
int ok=0;switch(word) {
default:return 0;
}
if(ok)vu_aot_counts[0]++;return ok;
}
static int vu_aot_has_lower(uint32_t word) {
switch(word) {
default:return 0;
}
}
static int vu_aot_try_lower(uint32_t vf[32][4],uint32_t *vi,uint8_t *mem,uint32_t mem_mask,uint32_t pc,uint32_t *branch_delay,uint32_t *branch_target,uint32_t word) {
if(!gekko2_opt_enabled(GEKKO2_OPT_VU_JIT))return 0;
int ok=0;switch(word) {
default:return 0;
}
if(ok)vu_aot_counts[1]++;return ok;
}
static int vu_aot_try_timed_upper(uint32_t vf[32][4],uint32_t *vi,uint32_t acc[4],uint32_t word,uint32_t *mac,uint32_t *status) {
if(!gekko2_opt_enabled(GEKKO2_OPT_VU_JIT))return 0;
int ok=0;switch(word&0x01ffffffu) {
default:return 0;
}
if(ok)vu_aot_counts[0]++;return ok;
}
static int vu_aot_available(uint32_t upper,uint32_t lower) {return gekko2_opt_enabled(GEKKO2_OPT_VU_JIT)&&(vu_aot_has_upper(upper&0x01ffffffu)||(!(upper&0x80000000u)&&vu_aot_has_lower(lower)));}
