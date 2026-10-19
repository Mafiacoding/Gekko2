#include "core/hw/frontend_settings.h"
#include <stdio.h>
#include <string.h>
void frontend_settings_defaults(frontend_settings *s)
{if(s)*s=(frontend_settings){0,1,1,0};}
static int valid(const frontend_settings *s)
{return s&&s->hud<=1&&s->throughput<=1&&s->fps<=1&&s->gx<=1;}
int frontend_settings_load(frontend_settings *s,const char *path)
{
 if(!s||!path)return -1;
 FILE *f=fopen(path,"r");char backup[256];
 if(!f&&snprintf(backup,sizeof backup,"%s.bak",path)<(int)sizeof backup)f=fopen(backup,"r");
 if(!f)return -1;
 char key[32],version[8],values[4][8],extra;
 int n=fscanf(f,"%31s %7s %7s %7s %7s %7s %c",key,version,values[0],values[1],values[2],values[3],&extra);
 int ok=!ferror(f);fclose(f);
 if(!ok||n!=6||strcmp(key,"GEKKO2_FRONTEND")||strcmp(version,"1"))return -1;
 unsigned bits[4];for(unsigned i=0;i<4;i++){
  if(strcmp(values[i],"0")&&strcmp(values[i],"1"))return -1;
  bits[i]=(unsigned)(values[i][0]-'0');
 }
 *s=(frontend_settings){bits[0],bits[1],bits[2],bits[3]};return 0;
}
int frontend_settings_save(const frontend_settings *s,const char *path)
{
 if(!valid(s)||!path)return -1;
 char temp[256],backup[256];
 if(snprintf(temp,sizeof temp,"%s.tmp",path)>=(int)sizeof temp||snprintf(backup,sizeof backup,"%s.bak",path)>=(int)sizeof backup)return -1;
 FILE *f=fopen(temp,"w");if(!f)return -1;
 int ok=fprintf(f,"GEKKO2_FRONTEND 1 %u %u %u %u\n",s->hud,s->throughput,s->fps,s->gx)>0;
 if(fclose(f))ok=0;if(!ok){remove(temp);return -1;}
 if(!rename(temp,path)){remove(backup);return 0;}
 remove(backup);if(rename(path,backup)){remove(temp);return -1;}
 if(rename(temp,path)){rename(backup,path);remove(temp);return -1;}
 remove(backup);return 0;
}
