#define _POSIX_C_SOURCE 200809L
#include "core/hw/frontend_log.h"
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

static int failed(frontend_log *log,int error)
{
 log->errors++;log->last_error=error?error:EIO;return -1;
}
int frontend_log_close(frontend_log *log,FILE *file)
{
 if(!log||!file)return -1;
 int error=ferror(file)?(errno?errno:EIO):0;
 if(fclose(file)!=0&&!error)error=errno?errno:EIO;
 return error?failed(log,error):0;
}
int frontend_log_begin(frontend_log *log,const char *directory,int disc,int gx)
{
 if(!log||!directory)return -1;
 log->path[0]=0;log->errors=0;log->last_error=0;
 if(mkdir(directory,0777)!=0) {
  struct stat st;
  if(errno!=EEXIST||stat(directory,&st)!=0||!S_ISDIR(st.st_mode))return failed(log,errno);
 }
 DIR *dir=opendir(directory);if(!dir)return failed(log,errno);
 struct dirent *entry;
 while((entry=readdir(dir))) {
  unsigned long id=0;
  if(sscanf(entry->d_name,"Gekko2-R1336-%10lu-",&id)==1&&id<=UINT32_MAX&&id>log->sequence)
   log->sequence=(uint32_t)id;
 }
 if(closedir(dir)!=0)return failed(log,errno);
 /* Exclusive reservation survives HBC/application restarts and never
  * overwrites another BIOS or game run, including a partially written log. */
 for(unsigned attempts=0;attempts<100000u;attempts++) {
  if(log->sequence==UINT32_MAX)return failed(log,EOVERFLOW);
  uint32_t next=++log->sequence;char path[sizeof(log->path)];
  int n=snprintf(path,sizeof(path),"%s/Gekko2-R1336-%010lu-%s-%s.log",directory,
    (unsigned long)next,disc?"disc":"bios",gx?"gx":"software");
  if(n<0||(unsigned)n>=sizeof(path))return failed(log,ENAMETOOLONG);
  int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0666);
  if(fd<0){if(errno==EEXIST)continue;return failed(log,errno);}
  FILE *file=fdopen(fd,"w");
  if(!file){int error=errno;close(fd);return failed(log,error);}
  memcpy(log->path,path,(unsigned)n+1u);
  fprintf(file,"SESSION checkpoint=R1336 id=%lu mode=%s GX=%d\n",
    (unsigned long)next,disc?"DISC":"BIOS",!!gx);
  return frontend_log_close(log,file);
 }
 return failed(log,EEXIST);
}
FILE *frontend_log_open(frontend_log *log)
{
 if(!log||!log->path[0])return NULL;
 FILE *file=fopen(log->path,"a");
 if(!file)failed(log,errno);
 return file;
}
