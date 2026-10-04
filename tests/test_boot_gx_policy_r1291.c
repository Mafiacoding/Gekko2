#include "core/hw/frontend_runtime.h"
#include <stdio.h>
int main(void)
{
 for(int due=0;due<2;due++)for(int active=0;active<2;active++)for(int first=0;first<2;first++)
  for(int hud=0;hud<2;hud++)for(int gx=0;gx<2;gx++) {
   (void)hud;(void)gx;
   if(frontend_probe_first_image(due,active,first)!=(due&&active&&!first))return 1;
   if(frontend_allow_gx_primitives(gx,first)!=(gx&&first))return 1;
  }
 puts("PASS 32 boot policies: image detection independent of HUD/GX; primitive opt-in inactive before first image");return 0;
}
