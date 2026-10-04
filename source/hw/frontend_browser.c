#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "core/hw/frontend_browser.h"
static int namecmp(const char *a,const char *b)
{
 while(*a&&*b){int x=tolower((unsigned char)*a++),y=tolower((unsigned char)*b++);if(x!=y)return x-y;}
 return (unsigned char)*a-(unsigned char)*b;
}
int frontend_browser_is_image(const char *name)
{const char *p=strrchr(name,'.');return p&&(!namecmp(p,".iso")||!namecmp(p,".bin"));}
static int order(const void *aa,const void *bb)
{
 const frontend_browser_entry *a=aa,*b=bb;
 if(a->directory!=b->directory)return a->directory?-1:1;
 return namecmp(a->name,b->name);
}
static int join(char *dst,size_t cap,const char *path,const char *name)
{int n=snprintf(dst,cap,"%s%s%s",path,path[0]&&path[strlen(path)-1]=='/'?"":"/",name);return n>=0&&(size_t)n<cap?0:-1;}
void frontend_browser_release(frontend_browser *b)
{if(b){free(b->entries);memset(b,0,sizeof(*b));}}
static int reserve(frontend_browser *b)
{
 if(b->count<b->capacity)return 0;
 unsigned capacity=b->capacity?b->capacity*2u:32u;
 if(capacity<b->capacity)return -1;
 frontend_browser_entry *entries=realloc(b->entries,(size_t)capacity*sizeof(*entries));
 if(!entries)return -1;b->entries=entries;b->capacity=capacity;return 0;
}
int frontend_browser_scan(frontend_browser *b,const char *path)
{
 if(!b||!path||strlen(path)>=sizeof(b->path))return -1;
 DIR *d=opendir(path);if(!d)return -1;
 if(!b->entries&&reserve(b)){closedir(d);return -1;}
 memmove(b->path,path,strlen(path)+1);b->count=b->selected=b->truncated=0;
 unsigned parent=strcmp(b->path,b->root)!=0;
 if(parent){strcpy(b->entries[0].name,"..");b->entries[0].directory=1;b->count=1;}
 struct dirent *e;
 while((e=readdir(d))) {
  if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
  if(strlen(e->d_name)>=sizeof(b->entries[0].name)){b->truncated=1;continue;}
  char full[FRONTEND_BROWSER_PATH];struct stat st;
  if(join(full,sizeof(full),b->path,e->d_name)||stat(full,&st))continue;
  unsigned dir=S_ISDIR(st.st_mode);
  if(!dir&&(!S_ISREG(st.st_mode)||!frontend_browser_is_image(e->d_name)))continue;
  if(reserve(b)){b->truncated=1;break;}
  frontend_browser_entry *v=&b->entries[b->count++];strcpy(v->name,e->d_name);v->directory=dir;
 }
 closedir(d);qsort(b->entries+parent,b->count-parent,sizeof(b->entries[0]),order);return 0;
}
int frontend_browser_init(frontend_browser *b,const char *root,const char *start)
{
 if(!b||!root||!start||strlen(root)>=sizeof(b->root))return -1;
 memset(b,0,sizeof(*b));strcpy(b->root,root);
 if(frontend_browser_scan(b,start)==0)return 0;
 return frontend_browser_scan(b,root);
}
int frontend_browser_up(frontend_browser *b)
{
 if(!b||!strcmp(b->path,b->root))return 0;
 char parent[FRONTEND_BROWSER_PATH];strcpy(parent,b->path);
 size_t n=strlen(parent),base=strlen(b->root);
 while(n>base&&parent[n-1]=='/')parent[--n]=0;
 char *p=strrchr(parent,'/');
 if(!p||((size_t)(p-parent)<base)){strcpy(parent,b->root);}else *p=0;
 return frontend_browser_scan(b,parent);
}
int frontend_browser_activate(frontend_browser *b,char *selected,size_t capacity)
{
 if(!b||b->selected>=b->count)return -1;
 frontend_browser_entry *e=&b->entries[b->selected];
 if(e->directory&&!strcmp(e->name,".."))return frontend_browser_up(b);
 char path[FRONTEND_BROWSER_PATH];if(join(path,sizeof(path),b->path,e->name))return -1;
 if(e->directory)return frontend_browser_scan(b,path);
 if(!selected||strlen(path)>=capacity||!frontend_browser_is_image(path))return -1;
 struct stat st;if(stat(path,&st)||!S_ISREG(st.st_mode))return -1;
 strcpy(selected,path);return 1;
}
