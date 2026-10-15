#include <stdio.h>
#include <stdint.h>
#include "core/hw/gs_gx.h"
int main(void) {
  unsigned tests=0,failures=0;
#define CHECK(c) do{tests++;if(!(c)){fprintf(stderr,"FAIL %u: %s\n",__LINE__,#c);failures++;}}while(0)
  CHECK(gs_gx_framebuffer_store(0x11223344u,0xaabbccddu,0,0,0)==0x11223344u);
  CHECK(gs_gx_framebuffer_store(0x11223344u,0xaabbccddu,0,1,0)==0x91223344u);
  CHECK(gs_gx_framebuffer_store(0x11223344u,0xaabbccddu,0xff000000u,1,0)==0xaa223344u);
  CHECK(gs_gx_framebuffer_store(0x11223344u,0xaabbccddu,0,0,1)==0xaa223344u);
  CHECK(gs_gx_framebuffer_store(0x11223344u,0xaabbccddu,0,1,1)==0xaa223344u);
  CHECK(gs_gx_framebuffer_store(0x11223344u,0xaabbccddu,0x00ff0000u,0,0)==0x11bb3344u);
  CHECK(gs_gx_framebuffer_store(0x11223344u,0xaabbccddu,0xffffffffu,1,0)==0xaabbccddu);
  printf("BETA12.17 GX framebuffer alpha/FBA/FBMSK: %u tests, %u failures\n",tests,failures);
  return !!failures;
}
