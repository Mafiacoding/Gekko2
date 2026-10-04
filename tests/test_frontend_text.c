#include <stdlib.h>
#include <stdio.h>
#include "core/hw/frontend_text.h"
#include "core/hw/gs_wii_output.h"
int main(void)
{
    const unsigned n=640*480/2;unsigned *p=malloc((n+2)*sizeof(*p));if(!p)return 2;
    unsigned base=gs_rgb8_pair_to_ycbcr(20,40,60,20,40,60);
    for(unsigned i=0;i<n+2;i++)p[i]=base;
    p[0]=0x12345678;p[n+1]=0xabcdef01;
    frontend_text_draw(p+1,640,480,0,0,1," ",240,240,240);
    for(unsigned i=1;i<=n;i++)if(p[i]!=base)return 3;
    frontend_text_draw(p+1,640,480,51,30,3,"PCSX2",240,240,240);
    unsigned changed=0,partial=0,ink=gs_rgb8_pair_to_ycbcr(240,240,240,240,240,240);
    for(unsigned i=1;i<=n;i++)if(p[i]!=base){changed++;if(p[i]!=ink)partial++;}
    if(changed<100||partial<100)return puts("FAIL coverage font has no antialiasing"),1;
    frontend_text_draw(p+1,640,480,-20,-10,3,"CLIP",240,20,20);
    frontend_text_draw(p+1,640,480,630,470,3,"CLIP",20,240,20);
    if(p[0]!=0x12345678||p[n+1]!=0xabcdef01)return puts("FAIL clipped framebuffer bounds"),1;
    unsigned first=p[1];frontend_text_draw(p+1,639,480,0,0,3,"A",240,240,240);
    if(p[1]!=first)return puts("FAIL odd width guard"),1;
    free(p);puts("PASS native coverage font, transparent spaces, antialiasing, clipping and paired XFB bounds");return 0;
}
