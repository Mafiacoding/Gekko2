#include <stdio.h>
#include <string.h>
#include "core/iso_loader.c"
static unsigned fail;
#define CHECK(c,m) do{if(!(c)){printf("FAIL: %s\n",m);fail++;}}while(0)
static void entry(uint8_t*p,const char*n,unsigned lba,unsigned size,int dir){unsigned len=strlen(n);p[0]=34+len+(len%2==0);p[2]=lba;p[10]=size;p[11]=size>>8;p[25]=dir?2:0;p[32]=len;memcpy(p+33,n,len);}
int main(void){
 FILE*f=tmpfile();uint8_t p[2048]={0};entry(p,"IRX",1,2048,1);fwrite(p,1,2048,f);
 memset(p,0,sizeof p);entry(p,"MCSERV.IRX;1",2,100,0);fwrite(p,1,2048,f);fflush(f);
 iso_image_t img={0};img.fp=f;img.opened=1;img.root_size=2048;img.physical_stride=2048;iso_dirent_t out;
 CHECK(!iso_find_path(&img,"cdrom:\\IRX\\MCSERV.IRX;1",&out)&&out.lba==2&&out.size==100,"walks raw game-style module path");
 CHECK(!iso_find_path(&img,"cdrom0:/IRX/MCSERV.IRX",&out),"accepts slash paths and leaf version fallback");
 CHECK(iso_find_path(&img,"host:/IRX/MCSERV.IRX",&out)!=0,"rejects another device prefix");
 CHECK(iso_find_path(&img,"IRX/MCSERV.IRX;1/OTHER",&out)!=0,"cannot descend through a file");
 CHECK(iso_find_path(&img,"IRX/../MCSERV.IRX;1",&out)!=0,"rejects parent traversal");
 CHECK(iso_find_path(&img,"IRX/MISSING",&out)!=0,"missing leaf fails");
 memset(p,0,sizeof p);p[0]=1;rewind(f);fwrite(p,1,2048,f);fflush(f);
 CHECK(iso_find_path(&img,"IRX/MCSERV.IRX;1",&out)!=0,"malformed directory record fails safely");
 fclose(f);printf("nested ISO path %s\n",fail?"FAIL":"PASS");return fail?1:0;
}
