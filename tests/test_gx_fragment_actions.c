#include <assert.h>
#include <stdio.h>
#include "core/hw/gs_gx_surface.h"
static int alpha(unsigned a,unsigned ref,unsigned test){switch(test){case 0:return 0;case 1:return 1;case 2:return a<ref;case 3:return a<=ref;case 4:return a==ref;case 5:return a>=ref;case 6:return a>ref;default:return a!=ref;}}
int main(void){unsigned checks=0;
 for(unsigned fmt=0;fmt<4;fmt++)for(unsigned test=0;test<8;test++)for(unsigned fail=0;fail<4;fail++)for(unsigned date=0;date<2;date++)for(unsigned dm=0;dm<2;dm++)for(unsigned ztest=0;ztest<4;ztest++)for(unsigned n=0;n<12;n++){
  unsigned psm=fmt==3?10:fmt,as=n*23%256,ad=n&1?128:0,zt=n%3+4,zs=n%4+4;
  gs_gx_pipeline p={.ate=1,.atst=test,.aref=128,.afail=fail,.date=date,.datm=dm,.ztest=1,.ztst=ztest,.zwrite=1,.blend=1,.pabe=n&2,.zpsm=psm};
  unsigned color=1,writealpha=1,depth=1;
  int zpass=ztest==1||(ztest==2&&zt>=zs)||(ztest==3&&zt>zs);
  if(!zpass||(date&&(psm==1||((ad>>7)&1)!=dm)))color=writealpha=depth=0;
  else if(!alpha(as,128,test)){if(fail==0)color=writealpha=depth=0;else if(fail==1)depth=0;else if(fail==2)color=writealpha=0;else writealpha=depth=0;}
  unsigned expected=color|(writealpha<<1)|(depth<<2)|((color&&(!p.pabe||(as&128)))?8:0);
  assert(gs_gx_fragment_actions(&p,psm,ad,as,zt,zs)==expected);checks++;
 }
 for(unsigned w=1;w<=2048;w*=2)for(unsigned h=1;h<=2048;h*=2){int expected=w>=4&&h>=4&&w<=1024&&h<=1024&&w*h<=524288;assert(gs_gx_native_texture_valid(w,h)==expected);}
 puts("PASS native texture bounds and GS fragment actions (24576 DATE/ATE/AFAIL/Z/PABE cases)");printf("checks=%u\n",checks);
}
