#include "core/recompiler/experimental_options.h"
#include <stdio.h>
#include <string.h>
static unsigned requested,active;
static const char *keys[GEKKO2_EXP_COUNT]={"GEKKO2_EE_CHAIN_RESIDENCY","GEKKO2_GS_EXTENDED_PSM"};
unsigned gekko2_experimental_available(void)
{
 unsigned mask=1u<<GEKKO2_EXP_PSM;
#if defined(GEKKO2_EE_CHAIN_RESIDENCY) && !defined(PCSX2WII_JIT_DISABLE)
 mask|=1u<<GEKKO2_EXP_CHAIN;
#endif
 return mask;
}
int gekko2_experimental_requested(unsigned n){return n<GEKKO2_EXP_COUNT&&!!(requested&(1u<<n));}
int gekko2_experimental_enabled(unsigned n){return n<GEKKO2_EXP_COUNT&&!!(active&(1u<<n));}
int gekko2_experimental_request(unsigned n,int on)
{
 if(n>=GEKKO2_EXP_COUNT||(on&&!(gekko2_experimental_available()&(1u<<n))))return 0;
 if(on)requested|=1u<<n;else requested&=~(1u<<n);return 1;
}
void gekko2_experimental_apply_cold_boot(void){active=requested&gekko2_experimental_available();}
int gekko2_experimental_load(unsigned n,const char *path)
{
 if(n>=GEKKO2_EXP_COUNT||!path)return -1;
 requested&=~(1u<<n); /* Missing/malformed files fail closed. */
 FILE *f=fopen(path,"r");char backup[256];
 if(!f&&snprintf(backup,sizeof backup,"%s.bak",path)<(int)sizeof backup)f=fopen(backup,"r");
 if(!f)return -1;
 char key[64],value[8],extra;
 int fields=fscanf(f,"%63s %7s %c",key,value,&extra);int ok=!ferror(f);fclose(f);
 if(!ok||fields!=2||strcmp(key,keys[n])||(strcmp(value,"0")&&strcmp(value,"1")))return -1;
 return gekko2_experimental_request(n,value[0]=='1')?0:-1;
}
int gekko2_experimental_save(unsigned n,const char *path)
{
 if(n>=GEKKO2_EXP_COUNT||!path)return -1;
 char temp[256],backup[256];
 if(snprintf(temp,sizeof temp,"%s.tmp",path)>=(int)sizeof temp||
    snprintf(backup,sizeof backup,"%s.bak",path)>=(int)sizeof backup)return -1;
 FILE *f=fopen(temp,"w");if(!f)return -1;
 int ok=fprintf(f,"%s %d\n",keys[n],gekko2_experimental_requested(n))>0;
 if(fclose(f))ok=0;
 if(!ok){remove(temp);return -1;}
 if(!rename(temp,path)){remove(backup);return 0;}
 remove(backup);
 if(rename(path,backup)){remove(temp);return -1;}
 if(rename(temp,path)){rename(backup,path);remove(temp);return -1;}
 remove(backup);return 0;
}
