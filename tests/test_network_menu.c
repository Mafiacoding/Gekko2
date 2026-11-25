#include "frontend.h"
#include <assert.h>
#include <string.h>
static char captured[12000];
static void rect(int x,int y,int w,int h,uint8_t r,uint8_t g,uint8_t b)
{(void)x;(void)y;(void)w;(void)h;(void)r;(void)g;(void)b;}
static void text(int x,int y,int scale,const char *s,int r,int g,int b)
{(void)x;(void)y;(void)scale;(void)r;(void)g;(void)b;assert(strlen(captured)+strlen(s)+2<sizeof captured);strcat(captured,s);strcat(captured,"\n");}
int main(void)
{
 ui_text_renderer=text;
 runtime_telemetry_settings active={{0xc0a80102,26890},1},pending=active;
 runtime_telemetry_status status={0};
 ui_settings_draw(rect,UI_SET_NETWORK,0,0,0,0,"");assert(strstr(captured,"Network logger"));captured[0]=0;
 assert(!runtime_telemetry_apply(&active));
 assert(runtime_telemetry_adjust(&pending,RT_EDIT_IP4,10));
 runtime_telemetry_settings still_active;runtime_telemetry_get_settings(&still_active);
 assert(still_active.endpoint.ip==active.endpoint.ip&&pending.endpoint.ip!=active.endpoint.ip);
 status.state=2;status.stage=RT_STAGE_READY;status.local_ip=0xc0a80103;
 ui_network_draw(rect,RT_EDIT_IP4,&pending,&active,&status,"");
 assert(strstr(captured,"PC target: 192.168.1.12")&&strstr(captured,"Active PC: 192.168.1.2:26890"));
 assert(strstr(captured,"SOCKET READY - NO PC ACK")&&!strstr(captured,"PC RECEIPT CONFIRMED"));
 captured[0]=0;status.acknowledged=1;
 ui_network_draw(rect,RT_EDIT_TEST,&pending,&active,&status,"");assert(strstr(captured,"PC RECEIPT CONFIRMED"));
 captured[0]=0;status.state=3;status.stage=RT_STAGE_DHCP;status.error=-6;
 ui_network_draw(rect,RT_EDIT_TEST,&pending,&active,&status,"");assert(strstr(captured,"DHCP error=-6")&&strstr(captured,"NETWORK FAILED"));
 captured[0]=0;status.state=0;status.config_reason=RT_CONFIG_INVALID;
 ui_network_draw(rect,RT_EDIT_TEST,&pending,&active,&status,"");assert(strstr(captured,"INVALID CONFIG - OFF"));
 puts("PASS network menu entry, pending/active IP separation, no false socket confirmation, ACK confirmation and DHCP/config errors");
}
