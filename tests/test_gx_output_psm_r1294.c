#include "core/hw/frontend_runtime.h"
#include <stdio.h>
int main(void){
 for(int r=0;r<2;r++)for(int f=0;f<2;f++)for(unsigned p=0;p<32;p++)
  if(frontend_allow_gx_output(r,f,p)!=(r&&f&&(p==0||p==1)))return 1;
 puts("PASS CT24/CT32 GX presentation; unsupported PSM and pre-image gates decline");return 0;
}
