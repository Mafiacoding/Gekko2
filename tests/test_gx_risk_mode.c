#include "core/hw/gs_gx_surface.h"
#include "core/recompiler/experimental_options.h"
#include "core/recompiler/optimization.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void){
 const unsigned option=GEKKO2_EXP_GX_RISK,bit=1u<<option;
 unsigned available=!!(gekko2_experimental_available()&bit);
 assert(!gekko2_experimental_requested(option)&&!gekko2_experimental_enabled(option));
 uint32_t normal=gekko2_optimization_mask;
 char dir[]="/tmp/gekko2-risk-XXXXXX",path[256];assert(mkdtemp(dir));snprintf(path,sizeof path,"%s/gx-risk.cfg",dir);
 assert(gekko2_experimental_request(option,1)==(int)available);
 assert(!gekko2_experimental_enabled(option));gekko2_experimental_apply_cold_boot();
 assert(gekko2_experimental_enabled(option)==(int)available);
 assert(!gekko2_experimental_save(option,path));assert(gekko2_experimental_request(option,0));
 assert(gekko2_experimental_enabled(option)==(int)available);
 assert(!gekko2_experimental_load(option,path));assert(gekko2_experimental_requested(option)==(int)available);
 assert(!gekko2_experimental_requested(GEKKO2_EXP_PSM)&&!gekko2_experimental_requested(GEKKO2_EXP_CHAIN));
 const char *bad[]={"GEKKO2_GX_RISK_MODE 2","GEKKO2_GX_RISK_MODE 01","GEKKO2_GX_RISK_MODE 1 extra","GEKKO2_GS_EXTENDED_PSM 1",""};
 for(unsigned n=0;n<sizeof bad/sizeof bad[0];n++){
  FILE *f=fopen(path,"w");assert(f&&fputs(bad[n],f)>=0&&!fclose(f));
  assert(gekko2_experimental_load(option,path)==-1&&!gekko2_experimental_requested(option));
  assert(gekko2_experimental_enabled(option)==(int)available);
 }
 gekko2_experimental_apply_cold_boot();assert(!gekko2_experimental_enabled(option));assert(gekko2_optimization_mask==normal);
 remove(path);rmdir(dir);
 gs_gx_texture_draw *d=calloc(1,sizeof(*d));assert(d);d->columns=d->rows=d->coverage.width=d->coverage.height=4;
 for(unsigned y=0;y<4;y++)d->coverage.right[y]=4;
 uint8_t actions[16];uint32_t blend,rows;unsigned checks=0;
 for(unsigned mask=0;mask<65536;mask++){
  for(unsigned n=0;n<16;n++)actions[n]=((mask>>n)&1)?GS_GX_WRITE_COLOR|GS_GX_WRITE_Z:GS_GX_WRITE_Z;
  rows=77;blend=99;int ok=gs_gx_risk_plan(d,actions,0,&blend,&rows);
  assert(ok==(mask==65535));assert(ok?(rows==4&&blend==0):(rows==77&&blend==99));checks++;
  for(unsigned n=0;n<16;n++)actions[n]=GS_GX_WRITE_COLOR|(((mask>>n)&1)?GS_GX_DO_BLEND:0);
  rows=77;blend=99;ok=gs_gx_risk_plan(d,actions,0,&blend,&rows);
  assert(ok==(mask==0||mask==65535));assert(ok?(rows==4&&blend==(mask?GS_GX_DO_BLEND:0)):(rows==77&&blend==99));checks++;
 }
 memset(actions,GS_GX_WRITE_COLOR,sizeof actions);
 assert(!gs_gx_risk_plan(d,actions,2,&blend,&rows)&&!gs_gx_risk_plan(d,actions,3,&blend,&rows));
 assert(gs_gx_risk_plan(d,actions,1,&blend,&rows));
 for(unsigned y=1;y<4;y++)d->coverage.right[y]=0;assert(!gs_gx_risk_plan(d,actions,0,&blend,&rows));
 d->coverage.right[1]=4;assert(gs_gx_risk_plan(d,actions,0,&blend,&rows)&&rows==2);
 d->coverage.right[1]=5;assert(!gs_gx_risk_plan(d,actions,0,&blend,&rows));
 free(d);printf("PASS GX risk: available=%u, factory OFF, cold-boot isolation/persistence, strict parsing, independent options; %u color/PABE masks, SCANMSK and bounded admission\n",available,checks);
}
